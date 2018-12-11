#include <omnetpp.h>

using namespace omnetpp;

class Queue : public cSimpleModule
{
  protected:
    cMessage *msgServiced;
    cMessage *endServiceMsg;

    cQueue queue;
    long total;
    long dropped;
    simtime_t congestionStart;
    simtime_t congestion;

    simsignal_t qlenSignal;
    simsignal_t busySignal;
    simsignal_t queueingTimeSignal;
    simsignal_t responseTimeSignal;
    simsignal_t droppedSignal;
    simsignal_t droppedPercSignal;
    simsignal_t timeCongestionSignal;

    double getDroppedPerc();
    bool isBlocked();

  public:
    Queue();
    virtual ~Queue();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Queue);


Queue::Queue()
{
    msgServiced = endServiceMsg = nullptr;
}

Queue::~Queue()
{
    delete msgServiced;
    cancelAndDelete(endServiceMsg);
}

void Queue::initialize()
{
    endServiceMsg = new cMessage("end-service");
    queue.setName("queue");

    qlenSignal = registerSignal("qlen");
    busySignal = registerSignal("busy");
    queueingTimeSignal = registerSignal("queueingTime");
    responseTimeSignal = registerSignal("responseTime");
    droppedSignal = registerSignal("dropped");
    droppedPercSignal = registerSignal("droppedPerc");
    timeCongestionSignal = registerSignal("timeCongestion");

    dropped = 0;
    total = 0;
    congestionStart = SIMTIME_ZERO;
    congestion = SIMTIME_ZERO;

    emit(qlenSignal, queue.getLength());
    emit(busySignal, false);
    emit(droppedSignal, dropped);
    emit(droppedPercSignal, getDroppedPerc());
    emit(timeCongestionSignal, congestion);
}

void Queue::handleMessage(cMessage *msg)
{
    if (msg == endServiceMsg) { // Self-message arrived

        EV << "Completed service of " << msgServiced->getName() << endl;
        send(msgServiced, "out");

        //Response time: time from msg arrival timestamp to time msg ends service (now)
        emit(responseTimeSignal, simTime() - msgServiced->getTimestamp());

        if (queue.isEmpty()) { // Empty queue, server goes in IDLE

            EV << "Empty queue, server goes IDLE" <<endl;
            msgServiced = nullptr;
            emit(busySignal, false);

        }
        else { // Queue contains users

            if(isBlocked()){
                EV << "Congestion: " << congestion << " start: " << congestionStart << endl;
                congestion += simTime() - congestionStart;
            }

            msgServiced = (cMessage *)queue.pop();

            emit(qlenSignal, queue.getLength()); //Queue length changed, emit new length!

            //Waiting time: time from msg arrival to time msg enters the server (now)
            emit(queueingTimeSignal, simTime() - msgServiced->getTimestamp());

            EV << "Starting service of " << msgServiced->getName() << endl;
            simtime_t serviceTime = par("serviceTime");
            scheduleAt(simTime()+serviceTime, endServiceMsg);

        }

    }
    else { // Data msg has arrived

        //Setting arrival timestamp as msg field
        msg->setTimestamp();
        total++;

        if (!msgServiced) { //No message in service (server IDLE) ==> No queue ==> Direct service

            ASSERT(queue.getLength() == 0);

            msgServiced = msg;
            emit(queueingTimeSignal, SIMTIME_ZERO);

            EV << "Starting service of " << msgServiced->getName() << endl;
            simtime_t serviceTime = par("serviceTime");;
            scheduleAt(simTime()+serviceTime, endServiceMsg);
            emit(busySignal, true);
        }
        else {  //Message in service (server BUSY) ==> Queuing
            EV << msg->getName() << " enters queue"<< endl;

            if (isBlocked()){
                EV << "Rejected " << msg->getName() << endl;
                emit(droppedSignal, ++dropped);
                delete(msg);
            }
            else{
                queue.insert(msg);

                if (isBlocked())
                    congestionStart = simTime();

                emit(qlenSignal, queue.getLength()); //Queue length changed, emit new length!
            }

            emit(droppedPercSignal, getDroppedPerc());

       }
    }
}

double Queue::getDroppedPerc(){
    if (total == 0) return 0;
    return (double)((double)dropped/(double)total);
}

bool Queue::isBlocked(){
    return queue.getLength() >= par("queueSize").longValue();
}
