import { Component, OnInit } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { HttpClientModule } from '@angular/common/http';
import { CommonModule } from '@angular/common';
import { TeamsService } from '../dynamic-dropdown/teams-service/teams.service';

interface Score {
  username: string;
  teamId: string;
  teamName?: string;
  score: number;
  scoreTime: number;
  formattedTime?: string;
}

interface Team {
  teamId: string;
  teamName: string;
}

@Component({
  selector: 'app-scoreboard',
  templateUrl: './scoreboard.component.html',
  styleUrls: ['./scoreboard.component.css'],
  standalone: true,
  imports: [CommonModule, HttpClientModule],
  providers: [TeamsService]
})
export class ScoreboardComponent implements OnInit {
  scores: Score[] = [];
  teams: Team[] = [];

  constructor(private http: HttpClient, private teamsService: TeamsService) {}

  ngOnInit(): void {
    this.http.get<Team[]>('https://2024.badge.bsidesroa.org/teams.json').subscribe(data => {
      this.teams = data.map(team => ({
        ...team
      }));
    });
    this.http.get<Score[]>('https://2024.badge.bsidesroa.org/scores.json').subscribe(data => {
      this.scores = data.map(score => ({
        ...score,
        formattedTime: this.formatTime(score.scoreTime),
        teamName: this.getTeamName(score.teamId)
      }));
    });
  }

  formatTime(epochSeconds: number): string {
    const date = new Date(epochSeconds * 1000);
    const hours = date.getHours().toString().padStart(2, '0');
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const seconds = date.getSeconds().toString().padStart(2, '0');
    return `${hours}:${minutes}:${seconds}`;
  }

  getTeamName(teamID: string): string {
    const team = this.teams.find(t => t.teamId === teamID);
    return team ? team.teamName : 'Unknown Team';
  }
}