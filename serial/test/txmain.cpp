#include "serialPort.h"
#include <string.h>
#include <io.h>

static void _usage(const char *prog) {
  printf("[Usage] %s [OPTION...]\n\n",prog);
  printf("  Main options:\n");
  printf("     -p COM       the COM port number\n");
  printf("     -b baudrate  the baudrate\n");
  printf("     -t timeout   the rx timeout (in millisecond)\n");
  printf("     -c cmd       transmit command; cmd could be in hexadecimal (0xXX) or decimal form (<=255)\n");
  printf("     -s data      transmit short data which is <= %d bytes\n",MAX_SHORT_PACKAGE_SIZE);
  printf("     -l data      transmit long data which is > %d bytes\n",MAX_SHORT_PACKAGE_SIZE);
  printf("     -f file      transmit a file\n");
  printf("     -h|-?        display this help\n\n");
  printf("  Auxiliary options for file or large data trasmission\n");
  printf("     -k size      maximal package size for the file or large data partition\n\n");
  printf("  Informative output:\n");
  printf("     -v           verbosely list transmission\n");
}

static int _getCMD(const char * data) {
  if(('0'==data[0])&&(('x'==data[1])||('X'==data[1]))) {
    int slen=strlen(data);

    if(slen<=4) {
      int i=2,cmd=0;

      for(;i<slen;i++) {
        int digit=0;

        if((data[i]>='0')&&(data[i]<='9'))       digit=(int)(data[i]-'0');
        else if((data[i]>='a')&&(data[i]<='f'))  digit=(int)(data[i]-'a')+10;
        else if((data[i]>='A')&&(data[i]<='F'))  digit=(int)(data[i]-'A')+10;
        else  return 256;  /* invalid command */
        cmd=(cmd<<4)+digit;
      }
      return cmd;
    } else  return 256;  /* invalid command */
  } else  return atoi(data);
}

static BOOL _getOptions(int argc,char **argv,int *lpCOM,int *lpBaud,int *lpTimeout,pTX_OPCODE_et lpOpcode,
                        char **data,int *lpDataSize,int *lpPackageSize,int *lpVerbose) {
  int i;

  /* the default settings */
  (*lpCOM)        =1;  /* COM1 */
  (*lpBaud)       =115200;
  (*lpTimeout)    =10000;  /* 10 seconds */
  (*lpOpcode)     =TX_OPCODE_UNKNOWN;
  (*data)         =NULL;
  (*lpDataSize)   =0;
  (*lpPackageSize)=0;
  (*lpVerbose)    =0;

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
          case 'c': case 'C':
            i++;
            if(i<argc) {
              if(TX_OPCODE_UNKNOWN==(*lpOpcode)) {
                (*lpOpcode)  =TX_OPCODE_CMD;
                (*lpDataSize)=_getCMD((const char *)argv[i]);
                if((*lpDataSize)>255) {
                  printf("[ERROR] Invalid command!\n");
                  return FALSE;
                }
              } else {
                printf("[ERROR] duplicated commands!\n");
                return FALSE;
              }
            }
            break;
          case 's': case 'S':
            i++;
            if(i<argc) {
              if(TX_OPCODE_UNKNOWN==(*lpOpcode)) {
                (*lpOpcode)  =TX_OPCODE_SDATA;
                (*data)      =argv[i];
                (*lpDataSize)=strlen((const char *)(*data));
              } else {
                printf("[ERROR] duplicated commands!\n");
                return FALSE;
              }
            }
            break;
          case 'l': case 'L':
            i++;
            if(i<argc) {
              if(TX_OPCODE_UNKNOWN==(*lpOpcode)) {
                (*lpOpcode)  =TX_OPCODE_LDATA;
                (*data)      =argv[i];
                (*lpDataSize)=strlen((const char *)(*data));
              } else {
                printf("[ERROR] duplicated commands!\n");
                return FALSE;
              }
            }
            break;
          case 'f': case 'F':
            i++;
            if(i<argc) {
              if(TX_OPCODE_UNKNOWN==(*lpOpcode)) {
                if(0==_access((const char *)argv[i],0)) {
                  (*lpOpcode)  =TX_OPCODE_LDATA;
                  (*data)      =argv[i];
                  (*lpDataSize)=0;
                } else {
                  printf("[ERROR] \"%s\" is not a valid file!\n",argv[i]);
                  return FALSE;
                }
              } else {
                printf("[ERROR] duplicated commands!\n");
                return FALSE;
              }
            }
            break;
          case 'k': case 'K':
            i++;
            if(i<argc)  (*lpPackageSize)=atoi((const char *)argv[i]);
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
  return (TX_OPCODE_UNKNOWN==(*lpOpcode))?FALSE:TRUE;
}

int main(int argc,char **argv) {
  int COM=0;
  int baudrate=0;
  int timeout=0;
  TX_OPCODE_et opcode=TX_OPCODE_UNKNOWN;
  char *data=NULL;
  int dataSize=0;
  int packageSize=0;
  int verbose=0;

  if(_getOptions(argc,argv,&COM,&baudrate,&timeout,&opcode,&data,&dataSize,&packageSize,&verbose)) {
    HANDLE hSerial=serialPortConnect(COM,baudrate,timeout);

    if(INVALID_HANDLE_VALUE==hSerial)  return 1;
    else {
      if(TX_OPCODE_CMD==opcode) {
        printf("Sending command \"0x%02X\"....%s!\n",(unsigned char)dataSize,serialPortTXcmd(hSerial,(unsigned char)dataSize)?"SUCCESS":"FAIL");
      } else if(TX_OPCODE_SDATA==opcode) {
      	printf("Sending string \"%s\"....%s\n",data,serialPortTXSData(hSerial,(unsigned char *)data,dataSize)?"SUCCESS":"FAIL");
      } else {  /* TX_OPCODE_LDATA==opcode */
        if(0==dataSize) {  /* transmit a file */
          printf("Sending file \"%s\"....%s\n",data,serialPortTXFile(hSerial,data,packageSize)?"SUCCESS":"FAIL");
        } else {  /* transmit a long string */
          printf("Sending string \"%s\"....%s\n",data,serialPortTXLData(hSerial,(unsigned char *)data,dataSize,packageSize)?"SUCCESS":"FAIL");
        }
      }
      serialPortClose(hSerial);
    }
  } else  _usage(argv[0]);
  return 0;
}
