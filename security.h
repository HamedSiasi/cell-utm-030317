/*
 * Copyright (c) 2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __SECURITY_H__
#define __SECURITY_H__
 
#include <inttypes.h>
 
#define MBED_DOMAIN          "e5728ced-5cfe-4812-833f-375eb9ba65d2"
#define MBED_ENDPOINT_NAME   "323b5137-37f7-4ca3-8cd6-d87e0810312a"

__attribute__((section("AHBSRAM1")))
const uint8_t SERVER_CERT[] = "-----BEGIN CERTIFICATE-----\r\n"
"MIIBmDCCAT6gAwIBAgIEVUCA0jAKBggqhkjOPQQDAjBLMQswCQYDVQQGEwJGSTEN\r\n"
"MAsGA1UEBwwET3VsdTEMMAoGA1UECgwDQVJNMQwwCgYDVQQLDANJb1QxETAPBgNV\r\n"
"BAMMCEFSTSBtYmVkMB4XDTE1MDQyOTA2NTc0OFoXDTE4MDQyOTA2NTc0OFowSzEL\r\n"
"MAkGA1UEBhMCRkkxDTALBgNVBAcMBE91bHUxDDAKBgNVBAoMA0FSTTEMMAoGA1UE\r\n"
"CwwDSW9UMREwDwYDVQQDDAhBUk0gbWJlZDBZMBMGByqGSM49AgEGCCqGSM49AwEH\r\n"
"A0IABLuAyLSk0mA3awgFR5mw2RHth47tRUO44q/RdzFZnLsAsd18Esxd5LCpcT9w\r\n"
"0tvNfBv4xJxGw0wcYrPDDb8/rjujEDAOMAwGA1UdEwQFMAMBAf8wCgYIKoZIzj0E\r\n"
"AwIDSAAwRQIhAPAonEAkwixlJiyYRQQWpXtkMZax+VlEiS201BG0PpAzAiBh2RsD\r\n"
"NxLKWwf4O7D6JasGBYf9+ZLwl0iaRjTjytO+Kw==\r\n"
"-----END CERTIFICATE-----\r\n";
 

__attribute__((section("AHBSRAM1")))
const uint8_t CERT[] = "-----BEGIN CERTIFICATE-----\r\n"
"MIIBzzCCAXOgAwIBAgIEeW72DzAMBggqhkjOPQQDAgUAMDkxCzAJBgNVBAYTAkZ\r\n"
"JMQwwCgYDVQQKDANBUk0xHDAaBgNVBAMME21iZWQtY29ubmVjdG9yLTIwMTYwHh\r\n"
"cNMTYxMDA3MTAxODU1WhcNMTYxMjMxMDYwMDAwWjCBoTFSMFAGA1UEAxNJZTU3M\r\n"
"jhjZWQtNWNmZS00ODEyLTgzM2YtMzc1ZWI5YmE2NWQyLzMyM2I1MTM3LTM3Zjct\r\n"
"NGNhMy04Y2Q2LWQ4N2UwODEwMzEyYTEMMAoGA1UECxMDQVJNMRIwEAYDVQQKEwl\r\n"
"tYmVkIHVzZXIxDTALBgNVBAcTBE91bHUxDTALBgNVBAgTBE91bHUxCzAJBgNVBA\r\n"
"YTAkZJMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEhQgE+rvkrc5QiWcguT02w\r\n"
"ar9ChFYHYHYnG8U7g7MVhrROYqH2O52i+h8CqGRnmmomRVWSSylTkQcwCQxKgFG\r\n"
"fDAMBggqhkjOPQQDAgUAA0gAMEUCIDlxe/X+EjcfIL6QXkSptKDXWeI1Ik4+nZW\r\n"
"kHpXfWOFQAiEAilXGCoOiAf3EGXUZVT+iwxjXsqE8z0PQqMp8tXgslo4=\r\n"
"-----END CERTIFICATE-----\r\n";
 

__attribute__((section("AHBSRAM1")))
const uint8_t KEY[] = "-----BEGIN PRIVATE KEY-----\r\n"
"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5vgQyhbjWoLeKMOT\r\n"
"vPuEBAM+148QlcBq9ANkQoknl86hRANCAASFCAT6u+StzlCJZyC5PTbBqv0KEVgd\r\n"
"gdicbxTuDsxWGtE5iofY7naL6HwKoZGeaaiZFVZJLKVORBzAJDEqAUZ8\r\n"
"-----END PRIVATE KEY-----\r\n";
 
#endif //__SECURITY_H__
