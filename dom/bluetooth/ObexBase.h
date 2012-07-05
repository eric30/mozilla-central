/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_obexbase_h__
#define mozilla_dom_bluetooth_obexbase_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

const char FINAL_BIT = 0x80;

enum ObexRequestCode {
  Connect = 0x80,
  Disconnect = 0x81,
  Put = 0x02,
  PutFinal = 0x82,
  Get = 0x03,
  GetFinal = 0x83,
  SetPath = 0x85,
  Abort = 0xFF
};

enum ObexResponseCode {
  Continue = 0x90,

  Success = 0xA0,
  Created = 0xA1,
  Accepted = 0xA2,
  NonAuthoritativeInfo = 0xA3,
  NoContent = 0xA4,
  ResetContent = 0xA5,
  PartialContent = 0xA6,

  MultipleChoices = 0xB0,
  MovedPermanently = 0xB1,
  MovedTemporarily = 0xB2,
  SeeOther = 0xB3,
  NotModified = 0xB4,
  UseProxy = 0xB5,

  BadRequest = 0xC0,
  Unauthorized = 0xC1,
  PaymentRequired = 0xC2,
  Forbidden = 0xC3,
  NotFound = 0xC4,
  MethodNotAllowed = 0xC5,
  NotAcceptable = 0xC6,
  ProxyAuthenticationRequired = 0xC7,
  RequestTimeOut = 0xC8,
  Conflict = 0xC9,
  Gone = 0xCA,
  LengthRequired = 0xCB,
  PreconditionFailed = 0xCC,
  RequestedEntityTooLarge = 0xCD,
  RequestUrlTooLarge = 0xCE,
  UnsupprotedMediaType = 0xCF,

  InternalServerError = 0xD0,
  NotImplemented = 0xD1,
  BadGateway = 0xD2,
  ServiceUnavailable = 0xD3,
  GatewayTimeout = 0xD4,
  HttpVersionNotSupported = 0xD5,

  DatabaseFull = 0xE0,
  DatabaseLocked = 0xE1,
};

int AppendHeaderName(char* retBuf, char* name, int length);
int AppendHeaderBody(char* retBuf, char* data, int length);
int AppendHeaderLength(char* retBuf, int objectLength);
int AppendHeaderConnectionId(char* retBuf, int connectionId);
void SetObexPacketInfo(char* retBuf, char opcode, int packetLength);

END_BLUETOOTH_NAMESPACE

#endif
