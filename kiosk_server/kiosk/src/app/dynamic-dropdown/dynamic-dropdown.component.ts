// biome-ignore lint/style/useImportType: <explanation>
import { Component, OnInit, Input, OnChanges, inject} from '@angular/core';
import { CommonModule } from '@angular/common';
import { TeamsService } from './teams-service/teams.service';
import { HttpClientModule } from '@angular/common/http';
// biome-ignore lint/style/useImportType: <explanation>
import {FormBuilder, FormControl, FormGroup, ReactiveFormsModule} from '@angular/forms';
// biome-ignore lint/style/useImportType: <explanation>
import { Team } from '../models/team.model';
import { BadgeApiService } from '../badge-api.service';

@Component({
  selector: 'app-dynamic-dropdown',
  templateUrl: './dynamic-dropdown.component.html',
  styleUrls: ['./dynamic-dropdown.component.css'],
  standalone: true,
  imports: [CommonModule, HttpClientModule, CommonModule, ReactiveFormsModule],
  providers: [TeamsService]
})
export class DynamicDropdownComponent implements OnInit, OnChanges {

  teamForm: FormGroup;
  dropdownData: Team[] = [];
  @Input() token = "";
  @Input() badge_id = "";
  @Input() username = "";
  @Input() teamId = "";
  message = "";
  badgeService: BadgeApiService = inject(BadgeApiService)

  constructor(private fb: FormBuilder, private teamsService: TeamsService) { 
    this.teamForm = this.fb.group({
      username: [this.badge_id],
      teamID: [this.teamId]
    });
  }

  ngOnInit(): void {
    this.teamsService.getData().subscribe(data => {
      this.dropdownData = data;
    });
  }
  ngOnChanges(): void {
    this.updateDropdown(this.teamId, this.username)
    console.log('change')
  }

    // Example: Update dropdown options on some event
  updateDropdown(inputTeamID:string, inputUsername:string) {
      this.teamForm.patchValue({ teamID: inputTeamID, username: inputUsername });
  }

  submitTeam() {
    const username = this.teamForm.get('username')?.value;
    console.log('Username:', username);
    const teamIDControl = this.teamForm.get('teamID');
    //Make sure the value exists and isn't null
    if (teamIDControl) {
      const selectedTeamID = teamIDControl.value;
      console.log('Selected Team ID:', selectedTeamID);
      // Handle form submission logic here
      this.badgeService.updateUser(this.badge_id, this.token, username, selectedTeamID)
      console.log("fire in the hole!")
    } else {
      console.error('teamID control is not found');
      this.badgeService.updateUser(this.badge_id, this.token, username)
      console.log("fire in the hole without a team!")
    }
  }
}



