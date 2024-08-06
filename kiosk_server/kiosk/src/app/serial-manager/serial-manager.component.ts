import { Component, EventEmitter, inject, Output, ViewChild } from '@angular/core';
import {MatExpansionModule, matExpansionAnimations, MatExpansionPanel} from '@angular/material/expansion';
import {MatBadgeModule} from '@angular/material/badge';
// biome-ignore lint/style/useImportType: <explanation>
import { SerialPort, SerialOptions } from "./serial";
import { BadgeApiService, dumpResponse } from '../badge-api.service';
// biome-ignore lint/style/useImportType: <explanation>
import { badgeData } from '../app.component';

@Component({
  selector: 'app-serial-manager',
  standalone: true,
  imports: [MatExpansionModule],
  templateUrl: './serial-manager.component.html',
  styleUrl: './serial-manager.component.css',
})

export class SerialManagerComponent {
  @Output() showUserForm: EventEmitter<badgeData> = new EventEmitter<badgeData>();
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  @Output() hideUserForm: EventEmitter<any> = new EventEmitter<void>();

  triggerShowUserForm() {
    // Call this function to show the identifiers
    if (this.badge_id && typeof this.username === 'string' && this.token && typeof this.teamId === 'string') {
      const data: badgeData = {
        username: this.username,
        token: this.token,
        badge_id: this.badge_id,
        teamId: this.teamId
      }
      this.showUserForm.emit(data);
    } else {
      console.log("Missing data to show form")
      //console.log(`username: ${this.username}\ntoken: ${this.token}\nbadge_id: ${this.badge_id}\nteamId: ${this.teamId}`)
    }

  }

  triggerHideUserForm() {
    // Call this function to show the identifiers
    this.hideUserForm.emit();
  }

  @ViewChild(MatExpansionPanel) sm_dialog: MatExpansionPanel | undefined; 
  version = 1.4
  title_text = `Click here to connect the badge reader. v${this.version}`;
  @Output() badge_id: string | undefined;
  transcript = "";
  port: SerialPort | undefined;
  reader: ReadableStreamDefaultReader | undefined;
  writer: WritableStreamDefaultWriter | undefined;
  decoder: TextDecoder | undefined;
  encoder: TextEncoder | undefined;
  keepReading = false;
  chunks_quantity: undefined | number = undefined
  chunks: {[key: string]: string} | undefined
  state = "start"
  @Output() token: string | undefined
  @Output() username: string | undefined
  @Output() teamId: string | undefined
  badgeService: BadgeApiService = inject(BadgeApiService)

  states: StateManager = {
    // Is there someone there?
    start: {
      send: "detect",
      expect: {
        "No Badge": {
          state: "start",
          finally: "init",
        },
        "Connected": {
          regex: /Connected.*\r?\nEOT\r?\n/s,
          state: "connected"
        }
      }
    },
    // We have contact
    connected: {
      first: "badge_id_parse",
      send: "dump",
      expect: {
        "No Badge": {
          state: "start",
          finally: "init",
        },  
        "Chunks" : {
          regex: /Chunks=[0-9]+\r?\nEOT/, 
          state: "chunk_processor",
        }
      }
    },
    // reading data
    "chunk_processor": {
      first: "find_chunks_quantity",
      expect: {
        "No Badge": {
          state: "start",
          finally: "init",
        },  
        "PART_" : {
          regex: /(?<=PART_[0-9]+\r?\n)((?!EOT).)*(?=\r?\nEOT)/ms,
          is_done: "parts_checker",
          state: "dump_data",
        }
      }
    },
    "dump_data": {
      first: "dump_badge",
      expect: {
        "": {
          state: "check_badge"
        }
      }
    },
    // end state
    "check_badge": {
      first: "sleep_delay",
      send: "detect",
      expect: {
        "No Badge": {
          state: "start",
          finally: "init",
        },
        // TODO check for badge_id
        "Connected": {
          state: "check_badge"
        }
      }
    }     
  }


  async init(): Promise<void> {
    //console.log(`Running INIT ${this.version}`)
    console.log("Initializing")
    this.transcript = ""
    this.badge_id = undefined
    this.chunks = undefined
    this.token = undefined
    this.triggerHideUserForm()
  }

  // determines if all parts are present.
  async parts_checker():Promise<boolean> {
    if (!this.chunks)
      this.chunks = {}
    // I hate this regex.
    for (const match of this.transcript.matchAll(/(?<=PART_(?<part_id>[0-9]+)\r?\n)(?<data>((?!EOT).)*)(?=\r?\nEOT)/gms)) {
      const part_id = match[1]
      const data = match[0]
      if (!(part_id in this.chunks))
        this.chunks[part_id] = data
    }
    //console.log(`Collected ${Object.keys(this.chunks).length}.  Looking for ${this.chunks_quantity}.`)
    if (Object.keys(this.chunks).length === this.chunks_quantity) {
      //console.log("Returning true")
      return true
    }
    //console.log("Returning false")
    return false
  }

  //sleep for 10 seconds
  async sleep_delay(): Promise<void> {
    await new Promise(resolve => setTimeout(resolve, 10000))
  }

  async disconnect() {
    if (this.port) {
      //cleanup
      this.keepReading = false
      if (this.reader) {
        await this.reader.cancel()
      }
      if (this.writer) {
        this.writer.releaseLock()
      }
      await this.port.close()
    }
    await this.init()
    this.port = undefined
    this.title_text = "Connection closed. Click here to connect the badge reader."
    console.log("Dialog closed")
    this.triggerHideUserForm()
  }
  
  async connect() {
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      const serial_manager: any = window.navigator;
      try {
          this.port = await serial_manager.serial.requestPort({
              filters: [
                  {usbVendorId: 0x2e8a},
              ],
            });
            await this.dump_loop();
          // Continue connecting to the device attached to |port|.
      } catch (e) {
        // The prompt has been dismissed without selecting a device.
        console.log("No port found, close dialog and update status");
        console.log(e)
        if (this.sm_dialog) {
          this.sm_dialog.close();
        } else {
          console.log("nothing found");
        }
      }
  }

  async dump_loop():Promise<undefined> {
    if (!this.port){
      await this.disconnect()
      return
    }
    await this.port.open({ baudRate: 9600 });
    let result:string;
    this.reader = this.port.readable.getReader();
    this.decoder = new TextDecoder();
    this.writer = this.port.writable.getWriter();
    this.encoder = new TextEncoder();

    this.keepReading = true
    while (this.keepReading) {

      // why are we here?
      if (!(this.state in this.states)) {
        console.log("Reached invalid state! ", this.state)
        console.log("Disconnecting")
        this.disconnect()
      }

      // set state
      const state_config = this.states[this.state]
      //console.log(`Starting state: ${this.state}`)

      if (state_config.first) {
        if (state_config.first === "badge_id_parse") 
          await this.badge_id_parse(this.transcript)
        if (state_config.first === "find_chunks_quantity")
          await this.find_chunks_quantity(this.transcript)
        if (state_config.first === "sleep_delay")
          await this.sleep_delay()
        if (state_config.first === "dump_badge") {
          console.log("Initiating dump")
          await this.dump_data()
        }
        //console.log(this.transcript)
      }
      // write
      if ("send" in state_config) {
        this.write_serial(`${state_config.send}\n`);
      }

      // read until end as determined by key, regex, or whatnot
      while (this.keepReading) {
        console.log("Reading")
        const { value, done } = await this.reader.read();
        if (done) {
          this.reader.releaseLock()
          this.disconnect()
          return
        }
        const read_message = this.decoder.decode(value)
        //console.log(`READ: ${read_message}`)
        this.transcript = this.transcript + read_message
        //console.log(`TRANSCRIPT:\n${this.transcript}`)

        let found = false
        for (const condition in state_config.expect) {
          const expect_config = state_config.expect[condition]
          //console.log(`enumerating ${condition}`)
          if (expect_config.is_done) {
            //console.log("Testing is_done")
            if (await this.parts_checker()) {
              //console.log(`passes to ${expect_config.state}`)
              found = true
              this.state = expect_config.state
            }
          } else if (expect_config.regex) {
            //console.log("Testing regex")
            //console.log(expect_config.regex)
            //console.log(expect_config.regex.test(this.transcript))
            if (expect_config.regex.test(this.transcript)) {
              //console.log(`passes to ${expect_config.state}`)
              found = true
              this.state = expect_config.state
            }
          } else {
            //console.log("Testing condition")
            if (this.transcript.includes(condition)) {
              //console.log(`passes to ${expect_config.state}`)
              found = true
              this.state = expect_config.state
            }
          }
          if (found) {
            if ("finally" in expect_config)
              await this.init()
            break
          }
        }
        //console.log(`${found} and ${this.keepReading}`)
        if (found)
          break
      }
    }
  }

  async badge_id_parse(transcript:string): Promise<void> {
    // regex out badge ID
    const id_finder = /Connected\ ([a-fA-F0-9]{8})/
    const response = id_finder.exec(transcript)
    if (response && response.length >= 2)
      this.badge_id = response[1]
    //console.log(`Badge ID is ${this.badge_id}`)
  }

  async find_chunks_quantity(transcript: string): Promise<void> {
    if (this.chunks_quantity)
      return
    const chunk_finder = /Chunks=([0-9]+)\r?\nEOT/
    const response = chunk_finder.exec(transcript)
    //console.log(`Chunks response ${response}`)
    if (response && response.length > 1)
      this.chunks_quantity = Number.parseInt(response[1])
  }

  async write_serial(message: string) {
    //console.log(`WRITING: ${message}`)
    if (this.writer && this.encoder) {
      await this.writer.ready;
      await this.writer.write(this.encoder.encode(message));
      //this.writer.releaseLock()
    }
  }

  async dump_data(): Promise<void> {
    if (this.badge_id && this.chunks) {
      const parts = this.getSortedValues(this.chunks)
      const response = await this.badgeService.putBadgeData(this.badge_id, parts)
      
      if ("token" in response) {
        this.token = response.token
        this.username = response.username
        this.teamId = response.teamId
        this.triggerShowUserForm()
        console.log("Badge Passed Validation");
      }

    } else {
      console.log("not ready to dump data")
      //console.log(this.badge_id)
      //console.log(this.chunks)
    }
  }

  getSortedValues(chunks: {[key: string]: string}): string[] {
    const entries = Object.entries(chunks).sort(([keyA], [keyB]) => {
      return keyA.localeCompare(keyB);
    });
    return entries.map(([key, value]) => value)
  }

}

interface StateManager {
  [key: string]: SingleStateDict
}

interface SingleStateDict {
  first?: string
  send?: string
  expect: {[key: string]: StateResponse}
}

interface StateResponse {
  state: string
  regex?: RegExp
  is_done?: string
  finally?: string
}



// 2e81:f00a