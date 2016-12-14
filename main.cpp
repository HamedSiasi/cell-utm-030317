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

//#define debug

#ifndef debug

#include "simpleclient.h"
#include "mbed.h"
#include "CellInterface.h"

//#define MBED_SERVER_ADDRESS "coap://api.connector.mbed.com:5684"
//#define MBED_SERVER_ADDRESS "coap://169.45.82.18:5684" //ARM LwM2M server
//#define MBED_SERVER_ADDRESS "coap://151.9.34.90:5683" //Sgonico CoAP echo server

#define MBED_SERVER_ADDRESS "coap://195.46.10.19:9005" //ARM LwM2M server



#define SIMPIN      NULL
#define APN         "giffgaff.com"
#define USERNAME    "giffgaff"
#define PASSWORD    NULL


// no attribute !
struct MbedClientDevice device = {
    "Manufacturer_String",      // Manufacturer
    "Type_String",              // Type
    "ModelNumber_String",       // ModelNumber
    "SerialNumber_String"       // SerialNumber
};


__attribute__((section("AHBSRAM0"))) MbedClient  mbed_client(device);
__attribute__((section("AHBSRAM0"))) DigitalOut  led1(LED1);


class LedResource {
public:

    LedResource() {
        // (1) Obj
        led_object = M2MInterfaceFactory::create_object("1" /*obj name*/);

        // (2) Instance
        M2MObjectInstance* led_inst = led_object->create_object_instance();

        // (3) Resouce
        M2MResource* status_res = led_inst->create_dynamic_resource(
        		"222"                         /*resource_name*/,
        		"status"                      /*resource_type*/,
				M2MResourceInstance::BOOLEAN  /*type*/,
				true                         /*observable*/
				                              /*multiple_instance*/);

        // (4) Read(GET) and Write(PUT)
        status_res->set_operation(M2MBase::GET_PUT_POST_DELETE_ALLOWED);

        // (5) Set the value of the resource
        status_res->set_value((const uint8_t*)"0"/*value*/, 1/*value_length*/);

        // (6) POST comes in, we want to execute the led_execute_callback
        status_res->set_execute_function( execute_callback(this, &LedResource::postHandling) );

        // (7) Completion of execute function can take a time, that's why delayed response is used
        status_res->set_delayed_response(true);
    }

    M2MObject* get_object() {
        return led_object;
    }

    void postHandling(void *argument) {
        led1 = !led1;
    }
private:
    M2MObject* led_object;
};



int main() {
    NetworkInterface *network_interface = 0;

    printf("Using Cellular Network\r\n\n");
    wait_ms(2000);

    CellInterface cell;
    cell.connect(APN, USERNAME, PASSWORD);
    network_interface = &cell;


    // -------------------- CoAP + LwM2M --------------------

    LedResource led_resource;

    mbed_client.create_interface(MBED_SERVER_ADDRESS, network_interface);

    M2MSecurity* register_object = mbed_client.create_register_object(); // server object specifying connector info
    M2MDevice*   device_object   = mbed_client.create_device_object();   // device resources object
    printf("Create list of Objects to register \r\n");


    M2MObjectList object_list;
    printf("Add objects to list \r\n");


    object_list.push_back(device_object);
    object_list.push_back(led_resource.get_object());


    // Set endpoint registration object
    mbed_client.set_register_object(register_object);


    // Register with mbed Device Connector
    mbed_client.test_register(register_object, object_list);


    printf("Register Done !\r\n\n\n");


    while (true)
    {
    	wait_ms(60000);
    	mbed_client.test_update_register();
        printf("Register update \r\n");
    }

    mbed_client.test_unregister();
}





//#include "mbed.h"
//
//DigitalOut myled(LED1);
//
//int main() {
//    while(1) {
//    	printf("a");
//        myled = 1;
//        wait(0.2);
//        myled = 0;
//        wait(0.2);
//    }
//}

#else

#include "mbed.h"
#include "GPS.h"
#include "MDM.h"

#define SIMPIN      NULL
#define APN         "giffgaff.com"
#define USERNAME    "giffgaff"
#define PASSWORD    NULL

int main(void)
{
    int ret;
    char buf[512] = "";

    // Create the modem object
    MDMSerial mdm; // use mdm(D1,D0) if you connect the cellular shield to a C027
    mdm.setDebug(4); // enable this for debugging issues

    // initialize the modem
    MDMParser::DevStatus devStatus = {};
    MDMParser::NetStatus netStatus = {};
    bool mdmOk = mdm.init(SIMPIN, &devStatus);
    mdm.dumpDevStatus(&devStatus);
    if (mdmOk) {
        // wait until we are connected
        mdmOk = mdm.registerNet(&netStatus);
        mdm.dumpNetStatus(&netStatus);
    }
    if (mdmOk)
    {
        // join the internet connection
        MDMParser::IP ip = mdm.join(APN,USERNAME,PASSWORD);
        if (ip == NOIP)
            printf("Not able to join network");
        else
        {
            mdm.dumpIp(ip);
            int port = 5683;
            const char* host = "ciot.it-sgn.u-blox.com";
            MDMParser::IP ip = mdm.gethostbyname(host);
            char data[] = "UDP SOCKET TEST";

                    printf("Testing UDP sockets with ECHO server\r\n");
                    int socket = mdm.socketSocket(MDMParser::IPPROTO_UDP, port);
                    if (socket >= 0)
                    {
                        mdm.socketSetBlocking(socket, 10000);
                        ret = mdm.socketSendTo(socket, ip, port, data, sizeof(data)-1);
                        if (ret == sizeof(data)-1) {
                            printf("Socket SendTo %s:%d " IPSTR " %d \"%s\"\r\n", host, port, IPNUM(ip), ret, data);
                        }
                        ret = mdm.socketRecvFrom(socket, &ip, &port, buf, sizeof(buf)-1);
                        if (ret >= 0) {
                            printf("Socket RecvFrom " IPSTR ":%d %d \"%.*s\" \r\n", IPNUM(ip),port, ret, ret,buf);
                        }
                        mdm.socketFree(socket);
                    }
            // disconnect
            mdm.disconnect();
        }
    }
    mdm.powerOff();
    return 0;
}
#endif
