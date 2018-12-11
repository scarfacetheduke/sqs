network Net
{
    submodules:
        gen: Source {
            parameters:
                @display("p=89,100");
        }
        sys: MSQueue {
            parameters:
                @display("p=329,100");
        }
        sink: Sink {
            parameters:
                @display("p=569,100");
        }
    connections:
        gen.out --> sys.in;
        sys.out --> sink.in;
}
