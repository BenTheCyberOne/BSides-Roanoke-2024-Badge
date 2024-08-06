import { Injectable } from '@angular/core';
// biome-ignore lint/style/useImportType: <explanation>
import { HttpClient } from '@angular/common/http';
// biome-ignore lint/style/useImportType: <explanation>
import { Observable } from 'rxjs';

@Injectable({
  providedIn: 'root'
})
export class TeamsService {
  private jsonUrl = 'https://2024.badge.bsidesroa.org/teams.json';

  constructor(private http: HttpClient) { }

  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  getData(): Observable<any> {
    // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    return this.http.get<any>(this.jsonUrl);
  }
}
