#include "ObexBase.h"

#include <string>

BEGIN_BLUETOOTH_NAMESPACE

int
AppendHeaderName(char* retBuf, char* name, int length)
{
  int headerLength = length + 3;

  retBuf[0] = 0x01;
  retBuf[1] = headerLength & 0xFF00;
  retBuf[2] = headerLength & 0x00FF;

  memcpy(&retBuf[3], name, length);

  return headerLength;
}

int
AppendHeaderBody(char* retBuf, char* data, int length)
{
  int headerLength = length + 3;

  retBuf[0] = 0x48;
  retBuf[1] = headerLength & 0xFF00;
  retBuf[2] = headerLength & 0x00FF;

  memcpy(&retBuf[3], data, length);

  return headerLength;
}


int
AppendHeaderLength(char* retBuf, int objectLength)
{
  retBuf[0] = 0xC3;
  retBuf[1] = objectLength & 0xFF000000;
  retBuf[2] = objectLength & 0x00FF0000;
  retBuf[3] = objectLength & 0x0000FF00;
  retBuf[4] = objectLength & 0x000000FF;

  return 5;
}

int
AppendHeaderConnectionId(char* retBuf, int connectionId)
{
  retBuf[0] = 0xCB;
  retBuf[1] = connectionId & 0xFF000000;
  retBuf[2] = connectionId & 0x00FF0000;;
  retBuf[3] = connectionId & 0x0000FF00;
  retBuf[4] = connectionId & 0x000000FF;

  return 5;
}

void
SetObexPacketInfo(char* retBuf, char opcode, int packetLength)
{
  retBuf[0] = opcode;
  retBuf[1] = packetLength & 0xFF00;
  retBuf[2] = packetLength & 0x00FF;
}

END_BLUETOOTH_NAMESPACE
