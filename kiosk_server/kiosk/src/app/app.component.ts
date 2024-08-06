import { Component } from '@angular/core';
import { CommonModule } from '@angular/common'; // Import CommonModule
import { SerialManagerComponent } from './serial-manager/serial-manager.component';
import { BadgeApiService } from './badge-api.service';
import { DynamicDropdownComponent } from './dynamic-dropdown/dynamic-dropdown.component';
import { ScoreboardComponent } from './scoreboard/scoreboard.component';


@Component({
  selector: 'app-root',
  standalone: true,
  imports: [CommonModule, SerialManagerComponent, DynamicDropdownComponent, ScoreboardComponent],
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css'],
})
export class AppComponent {
  badge_id = ""
  token = ""
  teamId = ""
  username = ""

  showUserFormVar = false; // Initial value is false, so the component is hidden by default

  public showUserForm(data: badgeData) {
    this.showUserFormVar = true;
    this.badge_id = data.badge_id
    this.token = data.token
    this.username = data.username
    this.teamId = data.teamId
    console.log("showing form");
  }

  public hideUserForm() {
    this.showUserFormVar = false;
    this.badge_id = ""
    this.token = ""
    this.username = ""
    this.teamId = ""
    console.log("hiding form");
  }
}

export interface badgeData {
  badge_id: string
  token: string
  teamId: string
  username: string
}