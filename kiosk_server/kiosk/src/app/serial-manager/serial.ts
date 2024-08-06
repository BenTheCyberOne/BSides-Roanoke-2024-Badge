export type SerialOptions = {
    baudRate?: number;
    dataBits?: number;
    stopBits?: number;
    parity?: string;
    bufferSize?: number;
    rtscts?: boolean;
    xon?: boolean;
    xoff?: boolean;
    xany?: boolean;
  };
  
  interface SerialPortInfo {
    readonly serialNumber: string;
    readonly manufacturer: string;
    readonly locationId: string;
    readonly vendorId: string;
    readonly vendor: string;
    readonly productId: string;
    readonly product: string;
  }
  
  export interface SerialPort {
    open(options: SerialOptions): Promise<void>;
    close(): Promise<void>;
    readonly readable: ReadableStream;
    readonly writable: WritableStream;
    readonly in: ReadableStream;
    readonly out: WritableStream;
    getInfo(): SerialPortInfo;
  }
  
  declare global {
    interface Window {
      connect250: () => void;
      connectMtr4: () => void;
      connectEscan: () => void;
      disconnect250: () => void;
      disconnectMtr4: () => void;
      disconnectEscan: () => void;
    }
  
    export const ParityType: {
      NONE: "none";
      EVEN: "even";
      ODD: "odd";
      MARK: "mark";
      SPACE: "space";
    };
  
    interface Navigator {
      serial: {
        //onconnect: EventHandlerNonNull;
        //ondisconnect: EventHandlerNonNull;
        onconnect: () => string;
        ondisconnect: () => string;
        requestPort(options: SerialOptions): Promise<SerialPort>;
        getPorts(): Promise<Iterable<SerialPort>>;
      };
    }
  }
  