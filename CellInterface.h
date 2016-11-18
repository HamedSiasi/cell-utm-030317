/* CellularInterface
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CELL_INTERFACE_H
#define CELL_INTERFACE_H

#include "nsapi.h"
#include "CellularInterface.h"


//class NetworkStack;

class CellInterface : public CellularInterface
{
public:
	CellInterface();
	~CellInterface();

	virtual int connect(const char *apn = 0, const char *username = 0, const char *password = 0);

	virtual int disconnect();

    virtual const char *get_ip_address();

    virtual const char *get_mac_address();


protected:

    virtual NetworkStack *get_stack();
};


#endif


