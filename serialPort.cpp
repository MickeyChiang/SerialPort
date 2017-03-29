#include <sys/types.h>  
#include <sys/stat.h>  
#include "serialPort.h"

#define INT_TO_4BYTES(_data_,_idx_,_value_)             \
  do {                                                  \
    (_data_)[(_idx_)  ]=(unsigned char)((_value_)>>24); \
    (_data_)[(_idx_)+1]=(unsigned char)((_value_)>>16); \
    (_data_)[(_idx_)+2]=(unsigned char)((_value_)>>8);  \
    (_data_)[(_idx_)+3]=(unsigned char) (_value_);      \
  } while(0)

static DWORD _getBaudRate(int baudrate) {
  return (256000==baudrate)?CBR_256000:(
         (128000==baudrate)?CBR_128000:(
         (115200==baudrate)?CBR_115200:(
         ( 57600==baudrate)?CBR_57600:(
         ( 56000==baudrate)?CBR_56000:(
         ( 38400==baudrate)?CBR_38400:(
         ( 19200==baudrate)?CBR_19200:(
         ( 14400==baudrate)?CBR_14400:(
         (  9600==baudrate)?CBR_9600:(
         (  4800==baudrate)?CBR_4800:(
         (  2400==baudrate)?CBR_2400:(
         (  1200==baudrate)?CBR_1200:(
         (   600==baudrate)?CBR_600:(
         (   300==baudrate)?CBR_300:(
         (   110==baudrate)?CBR_110:0))))))))))))));
}

static BOOL _dataWrite(HANDLE hFile,unsigned char *data,int size) {
  DWORD bytes_written=0;
  unsigned char cs=serialPortChecksum(data,size);

  data[size]=cs;
  size++;
  if(serialPortWrite(hFile,(LPCVOID)data,size,&bytes_written)) {
    if(size==bytes_written) {
      unsigned char response=0;

      if(serialPortRead(hFile,(LPVOID)&response,1,&bytes_written)) {
        if(response==cs)  return TRUE;
      }
    }
  }
  return FALSE;
}

unsigned char serialPortChecksum(unsigned char *data,int num) {
  int i=0;
  unsigned char checksum=0;

  for(;i<num;i++)  checksum+=(unsigned char)data[i];
  return checksum;
}

HANDLE serialPortConnect(int dev_id,int baudrate,int rxTimeout) {
  LPCWSTR p=NULL;
  WCHAR wsz[64];
  HANDLE hSerial=INVALID_HANDLE_VALUE;

  swprintf_s(wsz,63,L"\\\\.\\COM%d",dev_id);
  p=(LPCWSTR)wsz;

  printf("Opening serial port: COM%d",dev_id);
  hSerial=CreateFile(p,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

  if(INVALID_HANDLE_VALUE==hSerial)  printf(" [Error]\n");
  else {
    DCB dcbSerialParams={0};

    printf(" [OK]\n");
    /* Set device parameters (115200 baud, 1 start bit, 1 stop bit, no parity) */
    dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
    if(0==GetCommState(hSerial,&dcbSerialParams))  printf("Error getting device state\n");
    else {
      DWORD dBaudrate=_getBaudRate(baudrate);

      if(0==dBaudrate)  printf("Invalid baudrate\n");
      else {
        dcbSerialParams.BaudRate=dBaudrate;
        dcbSerialParams.ByteSize=8;
        dcbSerialParams.StopBits=ONESTOPBIT;
        dcbSerialParams.Parity  =NOPARITY;
        if(0==SetCommState(hSerial,&dcbSerialParams))  printf("Error setting device parameters\n");
        else {
          COMMTIMEOUTS timeouts={0};

          /* Set COM port timeout settings */
          timeouts.ReadIntervalTimeout        =0;
          timeouts.ReadTotalTimeoutMultiplier =0;
          timeouts.ReadTotalTimeoutConstant   =rxTimeout;
          timeouts.WriteTotalTimeoutConstant  =50;
          timeouts.WriteTotalTimeoutMultiplier=10;

          if(0==SetCommTimeouts(hSerial,&timeouts))  printf("Error setting timeouts\n");
          else                                       return hSerial;
        }
      }
    }
    CloseHandle(hSerial);
  }
  return INVALID_HANDLE_VALUE;
}

void serialPortClose(HANDLE hFile) {
  if(hFile!=INVALID_HANDLE_VALUE) {
    CloseHandle(hFile);
  }
}

BOOL serialPortWrite(HANDLE hFile,LPCVOID lpBuffer,DWORD nNumberOfBytesToWrite,LPDWORD lpNumberOfBytesWritten) {
  if((nNumberOfBytesToWrite>0)&&(hFile!=INVALID_HANDLE_VALUE)) {
    return WriteFile(hFile,lpBuffer,nNumberOfBytesToWrite,lpNumberOfBytesWritten,NULL);
  }
  return FALSE;
} 

BOOL serialPortRead(HANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead) {
  if((nNumberOfBytesToRead>0)&&(hFile!=INVALID_HANDLE_VALUE)) {
    return ReadFile(hFile,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,NULL);
  }
  return FALSE;
} 

BOOL serialPortTXcmd(HANDLE hFile,unsigned char cmd) {
  if(hFile!=INVALID_HANDLE_VALUE) {
    unsigned char data[3]={TX_OPCODE_CMD,0x00,0x00};

    data[1]=cmd;
    return _dataWrite(hFile,data,2);
  }
  return FALSE;
}

BOOL serialPortTXSData(HANDLE hFile,unsigned char *sdata,int size) {
  if((hFile!=INVALID_HANDLE_VALUE)&&(sdata!=NULL)&&(size>0)) {
    unsigned char data[3]={TX_OPCODE_SDATA,0x00,0x00};

    if(size>MAX_SHORT_PACKAGE_SIZE)  size=MAX_SHORT_PACKAGE_SIZE;
    data[1]=size;
    if(_dataWrite(hFile,data,2)) {
      unsigned char *package=(unsigned char *)malloc(sizeof(unsigned char)*(size+2));

      if(package) {
        BOOL status=FALSE;
        memcpy((void *)&(package[1]),(const void *)sdata,size);
        package[0]=TX_OPCODE_SDATA_CONTENT;
        status=_dataWrite(hFile,package,size+1);
        free(package);
        return status;
      }
    }
  }
  return FALSE;
}

BOOL serialPortTXLData(HANDLE hFile,unsigned char *sdata,int size,int package_size) {
  if((hFile!=INVALID_HANDLE_VALUE)&&(sdata!=NULL)&&(size>0)) {
    if((package_size>size)||(package_size<=0))  package_size=size;
    if((size<=MAX_SHORT_PACKAGE_SIZE)&&(package_size==size))  return serialPortTXSData(hFile,sdata,size);
    else {
      unsigned char *package=(unsigned char *)calloc((size_t)(package_size+LARGE_PACKAGE_HEADER_SIZE+1),sizeof(unsigned char));  /* op + #package + next size + data + cs */

      if(package) {
        unsigned char data[3]={TX_OPCODE_LDATA,0x00,0x00};
        BOOL status=FALSE;

        data[1]=(unsigned char)size;
        if(_dataWrite(hFile,data,2)) {
          unsigned char header[14]={TX_OPCODE_LDATA_HEADER,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
          int numOfPackages=(size+package_size-1)/package_size;

          /* the size of the input data */
          INT_TO_4BYTES(header,1,size);
          /* the number of the packages */
          INT_TO_4BYTES(header,5,numOfPackages);
          /* the size of the next package */
          INT_TO_4BYTES(header,9,package_size);

          if(_dataWrite(hFile,header,13)) {
            int i=0,idx=0;

            package[0]=TX_OPCODE_LDATA_PKG;
            /* package 0 ~ package (n-2) */
            for(--numOfPackages;i<numOfPackages;i++,idx+=package_size) {
              INT_TO_4BYTES(package,1,i);
              size-=package_size;
              if(size<package_size)  INT_TO_4BYTES(package,5,size);
              else                   INT_TO_4BYTES(package,5,package_size);
              memcpy((void *)&(package[LARGE_PACKAGE_HEADER_SIZE]),(const void *)&(sdata[idx]),package_size);
              if(!_dataWrite(hFile,package,package_size+LARGE_PACKAGE_HEADER_SIZE)) {
                free(package);
                return FALSE;
              }
            }
            /* the last package */
            INT_TO_4BYTES(package,1,i);
            INT_TO_4BYTES(package,5,0);
            memcpy((void *)&(package[LARGE_PACKAGE_HEADER_SIZE]),(const void *)&(sdata[idx]),size);
            status=_dataWrite(hFile,package,size+LARGE_PACKAGE_HEADER_SIZE);
          }
        }
        free(package);
        return status;
      }
    }
  }
  return FALSE;
}

BOOL serialPortTXFile(HANDLE hFile,char *filename,int package_size) {
  if((hFile!=INVALID_HANDLE_VALUE)&&(filename!=NULL)) {
    struct _stat statbuf;

    if(0==_stat((const char *)filename,&statbuf)) {
      FILE *fptr=NULL;

      if(0==fopen_s(&fptr,(const char *)filename,"rb")) {
        int size=statbuf.st_size;
        unsigned char *package=NULL;
        BOOL status=FALSE;

        if((package_size>size)||(package_size<=0))  package_size=size;

        package=(unsigned char *)calloc((size_t)(package_size+LARGE_PACKAGE_HEADER_SIZE+1),sizeof(unsigned char));  /* op + #package + next size + data + cs */
        if(package) {
          unsigned char data[3]={TX_OPCODE_LDATA,0x00,0x00};

          data[1]=(unsigned char)size;
          if(_dataWrite(hFile,data,2)) {
            unsigned char header[14]={TX_OPCODE_LDATA_HEADER,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
            int numOfPackages=(size+package_size-1)/package_size;

            /* the size of the input data */
            INT_TO_4BYTES(header,1,size);
            /* the number of the packages */
            INT_TO_4BYTES(header,5,numOfPackages);
            /* the size of the next package */
            INT_TO_4BYTES(header,9,package_size);

            if(_dataWrite(hFile,header,13)) {
              int i=0,idx=0;

              package[0]=TX_OPCODE_LDATA_PKG;
              /* package 0 ~ package (n-2) */
              for(--numOfPackages;i<numOfPackages;i++,idx+=package_size) {
                INT_TO_4BYTES(package,1,i);
                size-=package_size;
                if(size<package_size)  INT_TO_4BYTES(package,5,size);
                else                   INT_TO_4BYTES(package,5,package_size);
                if(package_size==fread_s((void *)&(package[LARGE_PACKAGE_HEADER_SIZE]),package_size,sizeof(unsigned char),package_size,fptr)) {
                  if(!_dataWrite(hFile,package,package_size+LARGE_PACKAGE_HEADER_SIZE)) {
                    free(package);
                    fclose(fptr);
                    return FALSE;
                  }
                } else {
                  free(package);
                  fclose(fptr);
                  return FALSE;
                }
              }
              /* the last package */
              INT_TO_4BYTES(package,1,i);
              INT_TO_4BYTES(package,5,0);
              if(size==fread_s((void *)&(package[LARGE_PACKAGE_HEADER_SIZE]),size,sizeof(unsigned char),size,fptr)) {
                status=_dataWrite(hFile,package,size+LARGE_PACKAGE_HEADER_SIZE);
              }
            }
          }
          free(package);
        }
        fclose(fptr);
        return status;
      }
    }
  }
  return FALSE;
}
