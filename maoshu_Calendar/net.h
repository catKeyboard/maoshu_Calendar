#ifndef __NET_H
#define __NET_H

#include <Arduino.h>

void netBegin();        // 连接WiFi(无凭证或失败则进入配网模式) + NTP校时
void netLoop();         // 维持连接/处理配网请求
bool netConnected();    // 是否已连上WiFi(站点模式)
String netDiag();       // 诊断信息(配网模式/IP/未连接)
bool inPortalMode();    // 是否处于配网模式

#endif
