#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

class MSQueue : public cSimpleModule
{
  protected:
    std::vector<cMessage*> aServiced; // Vector for messages in service
    std::vector<cMessage*> aEndMsg; // Vector for end service messages

    cQueue queue;
    long total;
    long dropped;
    long inService;
    simtime_t congestionStart;
    simtime_t congestion;
    double congestionTime;
    simtime_t busyStart;
    simtime_t busyTime;

    simsignal_t qlenSignal;
    simsignal_t busySignal;
    simsignal_t queueingTimeSignal;
    simsignal_t responseTimeSignal;
    simsignal_t droppedSignal;
    simsignal_t droppedPercSignal;
    simsignal_t timeCongestionSignal;
    simsignal_t avgUtilizationSignal;
    simsignal_t avgActiveServersSignal;

    // Methods
    cMessage* deQueue();
    double getDroppedPerc();
    double getActiveServers();
    bool isBlocked();
    bool canServe();
    void removeObj(std::vector<cMessage*> vector, cMessage* obj);
    cMessage* getServicedMessage(cMessage* end);
    bool isIdle();

  public:
    MSQueue();
    virtual ~MSQueue();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(MSQueue);


MSQueue::MSQueue() { } // Constructor

MSQueue::~MSQueue() // Destructor
{
    // TODO: Destroy every message inside the vectors

    // Resize the vectors to size 0
    aServiced.clear();
    aServiced.shrink_to_fit();

    aEndMsg.clear();
    aEndMsg.shrink_to_fit();
}

void MSQueue::initialize()
{
    queue.setName("queue");

    // Signal Registration
    qlenSignal = registerSignal("qlen");
    busySignal = registerSignal("busy");
    queueingTimeSignal = registerSignal("queueingTime");
    responseTimeSignal = registerSignal("responseTime");
    droppedSignal = registerSignal("dropped");
    droppedPercSignal = registerSignal("droppedPerc");
    timeCongestionSignal = registerSignal("timeCongestion");
    avgUtilizationSignal = registerSignal("avgUtilization");
    avgActiveServersSignal = registerSignal("avgActiveServers");

    // Initialize default values for the variables
    dropped = 0;
    total = 0;
    inService = 0;
    congestionStart = SIMTIME_ZERO;
    congestion = SIMTIME_ZERO;
    congestionTime = 0;
    busyStart = SIMTIME_ZERO;
    busyTime = SIMTIME_ZERO;

    // Update the stats
    emit(qlenSignal, queue.getLength());
    emit(busySignal, false);
    emit(droppedSignal, dropped);
    emit(droppedPercSignal, getDroppedPerc());
    emit(timeCongestionSignal, congestionTime);
    emit(avgUtilizationSignal, busyTime);
    emit(avgActiveServersSignal, getActiveServers());
}

void MSQueue::handleMessage(cMessage *msg)
{
    EV << "Name: " << msg->getName() << endl;

    if (strcmp(msg->getName(), "end-service") == 0) { // Self-message arrived

        auto serviced = getServicedMessage(msg); // Remove the message from the service

        EV << "Completed service of " << serviced->getName() << endl;
        inService--;

        emit(responseTimeSignal, simTime() - serviced->getTimestamp()); //Update the response time
        EV << "Owner: " << serviced->getOwner() << endl;
        send(serviced->dup(), "out"); // Send it to the out gate
        removeObj(aServiced, serviced); // Remove the message from the serviced vector
        delete(serviced); // Deallocate from memory

        if (queue.isEmpty()) { // Empty queue

            EV << "Empty queue" <<endl;

            // Clear the arrays
            if (inService == 0){
                aServiced.clear();
                aServiced.shrink_to_fit();
                aEndMsg.clear();
                aEndMsg.shrink_to_fit();
            }

            if (isIdle()) { // All servers are idle

                EV << "All servers are idle" << endl;

                // Update the stats
                emit(busySignal, false);
                emit(avgUtilizationSignal, busyTime / simTime());
                emit(avgActiveServersSignal, getActiveServers());
                busyTime += simTime() - busyStart;
            }
        }
        else { // MSQueue contains users

            if (isBlocked()){ // Limited queue is full
                // We terminate the congestion because we removed a user from the queue
                EV << "Congestion: " << congestion << " start: " << congestionStart << endl;
                // Update the total congestion time
                congestion += simTime() - congestionStart;
                emit(timeCongestionSignal, congestion / simTime());
            }

            auto toServe = deQueue(); // We extract a message from the queue basing on the chosen policy
            aServiced.push_back(toServe); // We add it to the serviced vector

            // Update the stats
            emit(qlenSignal, queue.getLength());
            emit(queueingTimeSignal, simTime() - toServe->getTimestamp());

            EV << "Starting service of " << toServe->getName() << endl;
            simtime_t serviceTime = par("serviceTime"); // Get a random service time based on the distribution
            auto endServiceMsg = new cMessage("end-service"); // Make a new end service message
            aEndMsg.push_back(endServiceMsg); // Add the new end service message to the vector
            inService++;
            scheduleAt(simTime()+serviceTime, endServiceMsg); // Schedule it
        }

        removeObj(aEndMsg, msg); // Remove the end service message from the array
        cancelAndDelete(msg); // Deallocate from memory
    }
    else { // Data message has arrived

        msg->setTimestamp();
        total++; // We increase the total number of received messages

        if (canServe()) { //There is an available server

            ASSERT(queue.getLength() == 0); // Make sure the queue is empty

            aServiced.push_back(msg); // We insert the message in the service array
            emit(queueingTimeSignal, SIMTIME_ZERO);

            EV << "Starting service of " << msg->getName() << endl;
            simtime_t serviceTime = par("serviceTime"); // Get a random service time based on the distribution
            auto endServiceMsg = new cMessage("end-service"); // Make a new end service message
            aEndMsg.push_back(endServiceMsg); // Add the new end service message to the vector
            inService++;
            scheduleAt(simTime()+serviceTime, endServiceMsg); // Schedule it

            // Update the stats
            emit(busySignal, true);
            emit(avgUtilizationSignal, busyTime / simTime());
            emit(avgActiveServersSignal, getActiveServers());
            busyStart = simTime();
        }
        else {  // Servers are busy, add to the queue

            if (isBlocked()){ // Queue is full
                EV << "Rejected " << msg->getName() << endl;
                emit(droppedSignal, ++dropped); // We increase the dropped counter
                delete(msg); // We delete the message from memory
            }
            else{ // Queue is not full

                EV << msg->getName() << " enters queue"<< endl;
                queue.insert(msg); // We insert the message in the queue

                if (isBlocked()) // If we filled the queue, start the congestion interval
                    congestionStart = simTime();

                emit(qlenSignal, queue.getLength()); // Update the queue length stat
            }

            emit(droppedPercSignal, getDroppedPerc()); // Update the dropped stat (in both cases, it's a percentage!)
       }
    }
}

cMessage* MSQueue::deQueue(){ // Returns the next message to be processed basing on the scheduling policy

    auto policy = par("policy").stringValue(); // Get the policy from the NED parameter
    cObject* msg;

    if (strcmp(policy, "FCFS") == 0){ // FCFS policy handling
        msg = queue.get(0); // Return the first element of the queue
    }
    else if (strcmp(policy, "LCFS") == 0) { // LCFS policy handling
        msg = queue.get(queue.getLength()-1); // We return the last element of the queue
    }
    else{
        EV << "The policy " << policy << " is not defined!" << endl;
        return nullptr;
    }

    return (cMessage*)queue.remove(msg);
}

double MSQueue::getDroppedPerc(){ // Returns the percentage of dropped users
    if (total == 0) return 0;
    return (double)((double)dropped/(double)total);
}

bool MSQueue::isBlocked(){ // Returns if the queue is full
    return queue.getLength() >= par("queueSize").longValue();
}

double MSQueue::getActiveServers(){ // Gets the mean active servers
    if (simTime().dbl() == 0) return 0;
    auto activePerc = busyTime.dbl() / simTime().dbl();
    if (par("infServers").boolValue()) return 0; // Infinite servers case, return 0
    else return activePerc*par("nbServer").longValue();
}

bool MSQueue::canServe(){ // Returns true if there is an available server
    if (par("infServers").boolValue()) // Infinite servers case, always available
            return true;
    else // Finite servers case
        return aServiced.size() <= par("nbServer").longValue();
}

cMessage* MSQueue::getServicedMessage(cMessage* end){ // Gets the serviced message related to the end service message
    if (end == nullptr) return nullptr;
    for(int i=0; i < aEndMsg.size(); i++){
        if (aEndMsg[i] == end)
            return (cMessage*)aServiced[i];
    }
    return nullptr; // We didn't find anything
}

void MSQueue::removeObj(std::vector<cMessage*> vector, cMessage* obj){ // Removes an object from a vector
    if (obj == nullptr) return;
    for (auto i = vector.begin(); i != vector.end(); ++i) {
        if ((cMessage*)&i == obj) {
            i = vector.erase(i);
            vector.shrink_to_fit();
            return;
        }
    }
    return;
}

bool MSQueue::isIdle(){ // Returns true if there is no message in service
    return aServiced.size() == 0;
}
