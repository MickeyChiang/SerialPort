#include "serialPort.h"

#define FOUR_BYTES_TO_INT(_data_,_idx_)  ((((int)(_data_)[(_idx_)]  )<<24)&0xFF000000)| \
                                         ((((int)(_data_)[(_idx_)+1])<<16)&0x00FF0000)| \
                                         ((((int)(_data_)[(_idx_)+2])<<8) &0x0000FF00)| \
                                         ( ((int)(_data_)[(_idx_)+3])     &0x000000FF)

static void _usage(const char *prog) {
  printf("[Usage] %s [OPTION...]\n\n",prog);
  printf("  Main options:\n");
  printf("     -p COM       the COM port number\n");
  printf("     -b baudrate  the baudrate\n");
  printf("     -t timeout   the rx timeout (in millisecond)\n");
  printf("     -h|-?        display this help\n\n");
  printf("  Informative output:\n");
  printf("     -v           verbosely list transmission\n");
}

static BOOL _getOptions(int argc,char **argv,int *lpCOM,int *lpBaud,int *lpTimeout,int *lpVerbose) {
  int i;

  /* the default settings */
  (*lpCOM)    =1;  /* COM1 */
  (*lpBaud)   =115200;
  (*lpTimeout)=10000;  /* 10 seconds */
  (*lpVerbose)=0;

  for(i=1;i<argc;i++) {
    if('-'==argv[i][0]) {
      if(0x00==argv[i][2]) {
        switch(argv[i][1]) {
          case 'p': case 'P':
            i++;
            if(i<argc)  (*lpCOM)=atoi((const char *)argv[i]);
            break;
          case 'b': case 'B':
            i++;
            if(i<argc)  (*lpBaud)=atoi((const char *)argv[i]);
            break;
          case 't': case 'T':
            i++;
            if(i<argc)  (*lpTimeout)=atoi((const char *)argv[i]);
            break;
          case 'v': case 'V':
            (*lpVerbose)=1;
            break;
          case 'h': case 'H': case '?':
            return FALSE;
          default:
            printf("[ERROR] Invalid option: %s\n",argv[i]);
            return FALSE;
        }
      } else {
        printf("[ERROR] Invalid option: %s!\n",argv[i]);
        return FALSE;
      }
    }
  }
  return TRUE;
}

/* "size" should consider to add 1 byte for checksum */
static BOOL _dataRead(HANDLE hSerial,unsigned char *data,int size,unsigned char *pcs,int verbose) {
  DWORD bytes_read=0;

  if(serialPortRead(hSerial,(LPVOID)data,size,&bytes_read)) {
    if(size==bytes_read) {
      (*pcs)=serialPortChecksum(data,size-1);

      if(data[size-1]==(*pcs)) {
        if(serialPortWrite(hSerial,(LPCVOID)pcs,1,&bytes_read)) {
          if(verbose)  printf("Read %d bytes....\n",size);
          return TRUE;
        } else  printf("[ERROR] Invalid read: fail to response!\n");
      } else  printf("[ERROR] Invalid read: checksum (0x%02X!=0x%02X)!\n",data[size-1],(*pcs));
    } else if(0==bytes_read) {
      if(verbose)  printf("Timeout....\n");
    } else  printf("[ERROR] Invalid read: size (%d)!\n",bytes_read);
  } else  printf("[ERROR] Invalid read: parameters!\n");
  return FALSE;
} 

static void _realPrint(const char *buffer,int size) {
  int i,j;
  char outstr[17];
 
  for(i=0,j=0;i<size;i++,j++) {
    printf("%02X ",(unsigned char)buffer[i]);
    outstr[j]=((buffer[i]>=0x20)&&(buffer[i]<=0x7F))?buffer[i]:'.';
    if(15==j) {
      outstr[j+1]=0x00;
      printf("    %s\n",outstr);
      j=-1;
    }
  }
  if(j<16) {
    outstr[j]=0x00;
    for(;j<16;j++)  printf("   ");
    printf("    %s\n",outstr);
  }
}

static void _printStr(const char *header,int head_size,const char *buffer,int size,int idx,unsigned char cs,int verbose) {
  if(verbose) {
    if(idx>=0)  printf("[%2d] Checksum: 0x%02X\n",idx,cs);
    else        printf("Checksum: 0x%02X\n",cs);
    if((header)&&(head_size>0)) {
      if(idx>=0)  printf("[%2d] Header: (%d bytes)\n",idx,head_size);
      else        printf("Header: (%d bytes)\n",head_size);
      _realPrint(header,head_size);
    }
  }
  if(idx>=0)  printf("[%2d] Receive string: (%d bytes)\n",idx,size);
  else        printf("Receive string: (%d bytes)\n",size);
  _realPrint(buffer,size);
}

int main(int argc,char **argv) {
  int COM=0;
  int baudrate=0;
  int timeout=0;
  int verbose=0;

  if(_getOptions(argc,argv,&COM,&baudrate,&timeout,&verbose)) {
    HANDLE hSerial=serialPortConnect(COM,baudrate,timeout);

    if(INVALID_HANDLE_VALUE==hSerial)  return 1;
    else {
      unsigned char cs=0x00;
      unsigned char opdata[3];

      for(;;) {
        if(_dataRead(hSerial,opdata,3,&cs,verbose)) {
          switch(opdata[0]) {
            case TX_OPCODE_CMD:
              if(verbose)  printf("Checksum: 0x%02X\n",cs);
              printf("Receive command: %02X\n",opdata[1]);
              break;
            case TX_OPCODE_SDATA:
              {
                int size=(int)((unsigned int)opdata[1])+2;
                unsigned char *buffer=(unsigned char *)malloc(sizeof(unsigned char)*size);

                if(buffer) {
                  if(_dataRead(hSerial,buffer,size,&cs,verbose)) {
                    if(TX_OPCODE_SDATA_CONTENT==buffer[0])  _printStr((const char *)buffer,1,(const char *)&buffer[1],size-2,-1,cs,verbose);
                    else  printf("[ERROR] Invalid opcode for short string transmission! (0x%02X)\n",buffer[0]);
                  }
                  free(buffer);
                } else  printf("[ERROR] No enough memory to receive the data!\n");
              }
              break;
            case TX_OPCODE_LDATA:
              {
                unsigned char header[14];

                if(_dataRead(hSerial,header,14,&cs,verbose)) {
                  if(TX_OPCODE_LDATA_HEADER==header[0]) {
                    int size           =FOUR_BYTES_TO_INT(header,1);
                    int numOfPackages  =FOUR_BYTES_TO_INT(header,5);
                    int nextPackageSize=FOUR_BYTES_TO_INT(header,9);

                    if((size<=0)||(numOfPackages<1)||(nextPackageSize<=0))  printf("[ERROR] Invalid header of the long string!\n");
                    else {
                      unsigned char *package=(unsigned char *)calloc((size_t)(nextPackageSize+LARGE_PACKAGE_HEADER_SIZE+1),sizeof(unsigned char));

                      if(package) {
                        int i=0;

                        for(;i<numOfPackages;i++) {
                          if(_dataRead(hSerial,package,nextPackageSize+LARGE_PACKAGE_HEADER_SIZE+1,&cs,verbose)) {
                            if(TX_OPCODE_LDATA_PKG==package[0]) {
                              _printStr((const char *)package,LARGE_PACKAGE_HEADER_SIZE,(const char *)&package[LARGE_PACKAGE_HEADER_SIZE],nextPackageSize,FOUR_BYTES_TO_INT(package,1),cs,verbose);
                              nextPackageSize=FOUR_BYTES_TO_INT(package,5);
                            } else {
                              printf("[ERROR] Invalid opcode for long string transmission! (0x%02X)\n",package[0]);
                              break;
                            }
                          }
                        }
                        free(package);
                      }
                    }
                  } else  printf("[ERROR] Invalid header of opcode for long string transmission! (0x%02X)\n",header[0]);
                }
              }
              break;
            default:
              printf("[ERROR] Invalid op-code: unknown (0x%02X)!\n",opdata[0]);
              break;
          }
        }
      }
      serialPortClose(hSerial);
    }
  } else  _usage(argv[0]);
  return 0;
}
