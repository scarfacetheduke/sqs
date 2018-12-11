simple MSQueue
{
    parameters:
        volatile double serviceTime @unit(s);
        volatile bool fifo;
        volatile int queueSize;
        volatile int nbServer;
        @display("i=block/queue;q=queue");
        
        @signal[droppedPerc](type="long");
        @signal[qlen](type="long");
        @signal[busy](type="bool");
        @signal[queueingTime](type="simtime_t");
        @signal[responseTime](type="simtime_t");
        @signal[dropped](type="long");
        @signal[timeCongestion](type="simtime_t");
        @signal[avgUtilization](type="simtime_t");
        @signal[avgActiveServers](type="simtime_t");
        
        @statistics[droppedPerc](title="percentage of dropped users"; record=vector,last; interpolationmode=none);
        @statistic[qlen](title="queue length";record=vector,timeavg,max;interpolationmode=sample-hold);
        @statistic[busy](title="server busy state";record=vector?,timeavg;interpolationmode=sample-hold);
        @statistic[queueingTime](title="queueing time at dequeue";unit=s;record=vector,mean,max;interpolationmode=none);
        @statistic[responseTime](title="response time at end of service";unit=s;record=vector,mean,max;interpolationmode=none);
        @statistic[timeCongestion](title="time congestion";unit=s;record=vector,mean,max;interpolationmode=none);
    	@statistic[dropped](title="dropped users";record=vector,last; interpolationmode=none);
    	@statistic[avgUtilization](title="avg utilization";unit=s;record=vector,mean,max;interpolationmode=none);
    	@statistic[avgActiveServers](title="avg active servers";unit=s;record=vector,mean,max;interpolationmode=none);
    gates:
        input in;
        output out;
}