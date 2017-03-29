#ifndef __SERIALPORT_H__
#define __SERIALPORT_H__

#include <windows.h>
#include <stdio.h>

#define LARGE_PACKAGE_HEADER_SIZE 64  /* The size of the header of the large package.                       */
                                      /* opcode + package id + next package size + dummy data  ==> 64 bytes */
                                      /* Also, 1 byte checksume will be appended after the data. So, the    */
                                      /* total size of large package will be 64 + sizeof(data) + 1 bytes.   */

#define MAX_SHORT_PACKAGE_SIZE    62  /* The target RX triggers an interrupt every 64-byte data.                    */
                                      /* The short package transimission contains 1-byte opcode and 1-byte checksum */

typedef enum TX_OPCODE_e {
  TX_OPCODE_UNKNOWN      =0x00,
  TX_OPCODE_CMD          =0x90,
  TX_OPCODE_SDATA        =0xA0,
  TX_OPCODE_SDATA_CONTENT=0xA1,
  TX_OPCODE_LDATA        =0xB0,
  TX_OPCODE_LDATA_HEADER =0xB1,
  TX_OPCODE_LDATA_PKG    =0xB2
} TX_OPCODE_et,*pTX_OPCODE_et;

#ifdef __cplusplus
extern "C" {
#endif

  unsigned char serialPortChecksum(unsigned char *data,int num);
  HANDLE        serialPortConnect(int dev_id,int baudrate,int rxTimeout);
  void          serialPortClose(HANDLE hFile);
  BOOL          serialPortWrite(HANDLE hFile,LPCVOID lpBuffer,DWORD nNumberOfBytesToWrite,LPDWORD lpNumberOfBytesWritten);
  BOOL          serialPortRead(HANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead);
  BOOL          serialPortTXcmd(HANDLE hFile,unsigned char cmd);
  BOOL          serialPortTXSData(HANDLE hFile,unsigned char *sdata,int size);
  BOOL          serialPortTXLData(HANDLE hFile,unsigned char *sdata,int size,int package_size);
  BOOL          serialPortTXFile(HANDLE hFile,char *filename,int package_size);

#ifdef __cplusplus
}
#endif

#endif  /* __SERIALPORT_H__ */
