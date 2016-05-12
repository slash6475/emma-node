Middleware emma-node
====================
Emma-node is a middleware cross-platform (including AVR, ARM, X86, MSP, etc)  for distributed Service Choreography over Wireless Sensor & Actor Network. Network communications are performed over 6LoWPAN with the dynamic routing RPL on which applications exchange CoAP transations. Its Ressource Oriented Architecture based on Restful specifications facilitates integration with other middlewares and systems based on Web technology. An emma-node provides a set of services which communicates through a Web ressource interface. This middleware provides by default three different kind of services to access to the system parameters  (/S/), to store variables or files (/L/) and to build Service Choreographies thanks to mobile agents (/A/).

Architecture
====================
Emma-node is composed of a CoAP server and client based on Erbium application of Contiki OS. A ressource manager forwards the incoming CoAP requests to the different service containers (denoted Root X). These containers encapsulates hardcoded processings which can use the CoAP client in order to send new requests in addition to response to their incoming one. They inherit a main class based on POSIX File API (Root-template). A JSON parser is used to read and build agents which are executed into the agent service container (Agent Root) whereas an arithmetic calculator is used by the local ressources to compute their value when the payload of incoming request contains arithmetic operations with internal ressource values.

<img src="https://github.com/slash6475/emma-node/blob/master/img/emma-node-uml.png">

### Web Service API
The default listenning port of emma-node is `5683`. Based on the Restful specification, the Web Services of an emma-node are state-less which facilitates their combinaison to form workflows. The different method available are listed below:

* `GET` returns the content of aressource
* `PUT` updates or compute the content of a ressource if the payload contains arithmetic operations.
* `POST` creates a new ressource
* `DELETE` deletes a ressource

Because the target platforms are limited in term of memory, the ressource content are stored in permanent memory in a tree-like organization which avoids also the storage of an index table such as illustrated in following Figure.
<img src="https://github.com/slash6475/emma-node/blob/master/img/emma-filesystem.png" width="700px">

Service Container
====================
All ressources are produced by internal node services encapsulated into container such as illustrated in following Figure. The addition of new hardcoded services on the nodes are facilitated thanks to a service container template based on POSIX File API. Therefore a ressource is considered like a file which can be open, read, write and close according to the method of incomming CoAP request. This implementation allows the processing of request chunk by chunk  like a stream. This approach saves considerably the RAM memory such as the requests do not need to be entirely stored on the node.

The service container provides data structure, memory management, concurrent access protection, the service processing (to implement) and a file system to store content in permanent memory. For example, the driver of a sensor or an actuator should be implemented into a service container in order to be accessible over its Input-Output ressources. Hence the ressources could be used by a Web browser or by agents in order to access to sensor value or to control actuator state.


<img src="https://github.com/slash6475/emma-node/blob/master/img/service-container.png">



System Service (/S/)
-------------
The particular System service (/S/) provides a set of Contiki OS parameters such as listed below:

* `/S/ns-uri` provides the URI of a JSON description file of this node.
* `/S/rand` provides a pseudo-random number.
* `/S/time` provides current running time (second).
* `/S/neighbors` provides the list of neighbours.
* `/S/routes` provides the routing table.
* `/S/ressources` provides the list of all ressources located on the node.

Local Service (/L/)
-------------

The local service provides storage space in permanent memory to store variables or files which be used by other services through the ressource interface.

Agent Service (/A/)
-------------
The Agent Service is a tiny Virtual Machine executing reactive agents defined by the Event-Condition-Action (ECA) model like an interaction rule. The `PRE` field defines the condition of activation based on the values of the internal ressources of the node. When an agent is triggered, it transmits the requests contained in the `TARGET` file with their corresponding payload stored in `POST` field. The payload can be defined as an arithmetic function of ressource values of the emitter and/or the receiver node  such as illustrated in following example. Because an agent is also a ressource, an agent can create, update or delete other agents including itself.

### Example: Control-Command Agent
The following agent sends a request to a light in order to increase its intensity (/S/light) according to the local value of a brightness sensor (/L/brightness) and a desired value (/L/ref).  If the value of `/S/brightness` is lower than a reference value stored in `/L/ref`, the agent sends a request to the node `[aaaa::2]` to increase its value of ressource `/L/light` by adding to its value the value of the brightness on emitter node divided by 10.

```json
{
	"PRE": "S#brightness < L#ref",
	"POST": [
		"L#brightness/10 + R#light"
	],
	"TARGET": [
		"PUT[aaaa::2]:5683/S/light"
	]
}
```

### Operators
The default operators implemented in Agent service are resumed below. Additionnal operators can be easily implemented thanks to an API.
* `!` logical NOT
* `||` logical OR
* `&&` logical AND
* `==` logical EQUAL
* `<` logical LOWER than
* `>` logical GREATER than
* `+` numerical SUM
* `-` numerical SUB
* `/` numerical DIVISION
* `*` numerical MULTIPLICATION
* `%` numerical MODULO
* `#` URI delimiter of variables
* `R#` Keyword for ressource reference on target
* `()` OPERAND


### Service Choregraphy
The following Figure presents an example of Service Choreography modelled by a Petri Network. Each rectangle represents a node in which are stored different ressources illustrated by circles. The agents are represented by the vertical bar or Petri Network transition. In this example, a switch sets a threshold value on a node containing a brightness sensor. The previous presented agent of control-command sends a request to a lamp to increase its value which forwards the request to another lamp until that ambiant brightness is greater than the configured by the switch.
<img src="https://github.com/slash6475/emma-node/blob/master/img/example-control-command.png" width="500px;">


### Example: Deployment Agent
Because an agent is also a ressource, it can deploy other agents. The following agent DiscoverDep is randomly triggered to broadcast itself to all neighbours and it deploys on them an other agent (DiscoverNot). DiscoverNot can be used for service discovery such as it will randomly be triggered to push the list of ressources, of routes and neighbours to a supervisor.
```json
{
    "NAME": "DiscoverDep",
    "PRE": "S#rand%3==0",
    "POST": [
        "A#DiscoverDep",
        {
            "PRE": "S#rand%3==0",
            "POST":[
                "{'ns-uri':'registry/ns-uri1.json','resources':S#resources,'routes':S#routes,'neighbors':S#neighbor}"
            ],
            "TARGET": [
                "PUT[aaaa::1]:5683/network"
            ]
        }
    ],
    "TARGET": [
        "POST[ff02::2]:5683/A/DiscoverDep",
        "POST[0::1]:5683/A/DiscoverNot"
    ]
}
```
### DNA-RNA Process
A Dynamic Network Agent (DNA) is a deployment agent responsible to accross a set of different nodes in order to install the different ressources (including the agents) for a given Service Choreography. The Residual Network Agent (RNA) is the final ressource mapping after that the deployment process is complete. A deployment agent can contain some other deployment agents like a Matroska such as illustrated in Figure below. In this example, the distributed application in bottom-right corner must be deployed according to the routing path (and so deployment path) of the right-up corner. Hence the different agents (p1, p2 and p3) are deployed by the agents (d0, d1 and d2). When d0 is triggered, it sends the agents p0 and d0 to node 0. When d0 is triggered, it sends the agents d1 and d2 to node 1. And finally, the agent d1 installs  the agent p2 on node 2 and respectively the agent d2 installs the agent p1 on the node 3.

<img src="https://github.com/slash6475/emma-node/blob/master/img/agent-deployment.png" width="500px">

HowTo
===========

Installation
-----------
```
bash install FOLDER
```

Programming
-----------
The software is implemented in C language and cross-platforms (MSP, AVR, ARM, X86, PIC, etc). The compilation must be executed in main project folder.
```
cd contiki/examples/emma-node-example/
```
Compilation
-----------
```
make all TARGET=avr-atmega128rfa1
avr-size --mcu=atmega128rfa1 emma-node-example.avr-atmega128rfa1  -C
```

Preparation
-----------
It is preferred to remove fuse and eeprom section to avoid to break fuse configuration and to erase programs stored in memories.
```
avr-objcopy -O ihex -R .eeprom -R .fuse -R .signature emma-node-example.avr-atmega128rfa1 emma-node-example.hex
```

Loading
-----------
Example to load the program over ISP connector with a JTAG2 device.
```
sudo avrdude -p atmega128rfa1 -c jtag2isp -P usb -Uflash:w:emma-node-example.hex
```
