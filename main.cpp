
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
#include "mbed.h"

#define MBED_SERVER_ADDRESS "coap://api.connector.mbed.com:5684"
//#define MBED_SERVER_ADDRESS "coap://169.45.82.18:5684"
#define CELLULAR_NETWORK 1

#ifdef CELLULAR_NETWORK
#include "MDM.h"
#define SIMPIN      NULL
#define APN         "giffgaff.com"
#define USERNAME    "giffgaff"
#define PASSWORD    NULL
#else
#include "EthernetInterface.h"
__attribute__((section("AHBSRAM0")))  EthernetInterface eth;
#endif


// no attribute !
struct MbedClientDevice device = {
    "Manufacturer_String",      // Manufacturer
    "Type_String",              // Type
    "ModelNumber_String",       // ModelNumber
    "SerialNumber_String"       // SerialNumber
};



__attribute__((section("AHBSRAM0")))   MbedClient  mbed_client(device);
__attribute__((section("AHBSRAM0")))   DigitalOut  led1(LED1);


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
        if (argument) {
            M2MResource::M2MExecuteParameter* param = (M2MResource::M2MExecuteParameter*)argument;
            String object_name = param->get_argument_object_name();
            uint16_t object_instance_id = param->get_argument_object_instance_id();
            String resource_name = param->get_argument_resource_name();
            int payload_length = param->get_argument_value_length();
            uint8_t* payload = param->get_argument_value();

            printf("[POST] Resource: %s/%d/%s executed\r\n", object_name.c_str(), object_instance_id, resource_name.c_str());
            printf("[POST] Payload: %.*s\r\n", payload_length, payload);
        }else{
        	printf("[POST] Received! NO Payload \r\n");
        }

        led1 = !led1;
    }

private:
    M2MObject* led_object;
};





int main() {

    NetworkInterface *network_interface = 0;
    int connect_success = -1;

	// -------------------- Transport layer --------------------

#ifdef CELLULAR_NETWORK
    printf("Using Cellular Network\r\n\n");

    wait_ms(3000);
    MDMSerial mdm;
    mdm.setDebug(4);
    if (!mdm.connect(SIMPIN, APN,USERNAME,PASSWORD))
    	return -1;


    //mdmUDPSocket socket;
    //socket.init();
    //socket.bind(5684);

    //network_interface = (NetworkInterface *)&socket;



#else
    printf("Using Ethernet\r\n");
    connect_success = eth.connect();
    network_interface = &eth;
    if(connect_success == 0){
    	printf("Connected to Network successfully\r\n");
    }
    else{
        printf("Connection to Network Failed %d! Exiting application....\r\n", connect_success);
        return 0;
    }
    const char *ip_addr = network_interface->get_ip_address();
    if (ip_addr){
        printf("IP address %s\r\n", ip_addr);
    }
    else{
        printf("No IP address\r\n");
    }
#endif


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


    printf("hooraa \r\n");
    while (true)
    {
    	wait_ms(10000);
    	printf("pre update \r\n");
        mbed_client.test_update_register();
        printf("update \r\n");
    }
    mbed_client.test_unregister();
    //status_ticker.detach();
}
