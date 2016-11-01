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

#include "simpleclient.h"
//#include <string>
//#include <sstream>
//#include <vector>
//#include "mbed-trace/mbed_trace.h"
//#include "mbedtls/entropy_poll.h"
#include "security.h"
#include "mbed.h"
//#include "rtos.h"
#include "EthernetInterface.h"

//__attribute__((section("AHBSRAM0")))
//EthernetInterface eth;

#define MBED_SERVER_ADDRESS "coap://api.connector.mbed.com:5684"

__attribute__((section("AHBSRAM0")))
struct MbedClientDevice device;

__attribute__((section("AHBSRAM0")))
MbedClient  mbed_client(device);
//__attribute__((section("AHBSRAM0")))
//Ticker      timer;
__attribute__((section("AHBSRAM0")))
DigitalOut  led1(LED1);



class LedResource {
public:

    LedResource() {
        // (1) Obj
        led_object = M2MInterfaceFactory::create_object("111" /*obj name*/);


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
        // Check if POST contains payload
        led1 = !led1;
    }

private:
    M2MObject* led_object;
};




// Network interaction must be performed outside of interrupt context
//__attribute__((section("AHBSRAM0")))
//Semaphore updates(0);
//volatile bool registered = false;
//volatile bool clicked = false;
//osThreadId mainThread;
//
//void unregister() {
//	printf("unregister \r\n");
//    registered = false;
//    updates.release();
//}
//
//void button_clicked() {
//	printf("button_clicked \r\n");
//    clicked = true;
//    updates.release();
//}



//Ticker status_ticker;



int main() {
//    unsigned int seed;
//    size_t len;
//
//#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT
//    // Used to randomize source port
//    mbedtls_hardware_poll(NULL, (unsigned char *) &seed, sizeof seed, &len);
//
//#elif defined MBEDTLS_TEST_NULL_ENTROPY
//
//#warning "mbedTLS security feature is disabled. Connection will not be secure !! Implement proper hardware entropy for your selected hardware."
//    // Used to randomize source port
//    mbedtls_null_entropy_poll( NULL,(unsigned char *) &seed, sizeof seed, &len);
//
//#else
//
//#error "This hardware does not have entropy, endpoint will not register to Connector.\
//You need to enable NULL ENTROPY for your application, but if this configuration change is made then no security is offered by mbed TLS.\
//Add MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES and MBEDTLS_TEST_NULL_ENTROPY in mbed_app.json macros to register your endpoint."
//#endif





    //srand(seed);
    //status_ticker.attach_us(blinky, 250000);

    // Keep track of the main thread
    //mainThread = osThreadGetId();
    //printf("Starting mbed Client example...\r\n");
    //mbed_trace_init();
    //mbed_trace_print_function_set(trace_printer);

//    NetworkInterface *network_interface = 0;
//    int connect_success = -1;
//    printf("Using Ethernet\r\n");
//
//    connect_success = eth.connect();
//    network_interface = &eth;
//
//    if(connect_success == 0)
//    {
//    	printf("Connected to Network successfully\r\n");
//    }
//    else
//    {
//        printf("Connection to Network Failed %d! Exiting application....\r\n", connect_success);
//        return 0;
//    }
//
//    const char *ip_addr = network_interface->get_ip_address();
//    if (ip_addr)
//    {
//        printf("IP address %s\r\n", ip_addr);
//    }
//    else
//    {
//        printf("No IP address\r\n");
//    }



    //---------------------------------------------------------------------------------------------------------------------



    // we create our button and LED resources
    LedResource led_resource;

    // Send update of endpoint resource values to connector every 15 seconds periodically
    //timer.attach(&button_clicked, 15.0);

    printf("1\r\n");
    mbed_client.create_interface(MBED_SERVER_ADDRESS, NULL);
    printf("2\r\n");
    M2MSecurity* register_object = mbed_client.create_register_object(); // server object specifying connector info
    printf("3\r\n");
    M2MDevice*   device_object   = mbed_client.create_device_object();   // device resources object
    printf("4\r\n");


    printf("Create list of Objects to register \r\n");
    M2MObjectList object_list;


    printf("Add objects to list \r\n");
    object_list.push_back(device_object);
    object_list.push_back(led_resource.get_object());

    // Set endpoint registration object
    printf("6\r\n");
    mbed_client.set_register_object(register_object);

    // Register with mbed Device Connector
    printf("7\r\n");
    mbed_client.test_register(register_object, object_list);
    //registered = true;




    printf("8\r\n");



//    while (true)
//    {
//        updates.wait(5000);
//
//        mbed_client.test_update_register();
//        printf("9\r\n");
//    }
//
//    mbed_client.test_unregister();
    //status_ticker.detach();
}
