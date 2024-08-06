import { Injectable } from '@angular/core';

@Injectable({
  providedIn: 'root'
})
export class BadgeApiService {

  readonly rootUrl = "https://2024-api.badge.bsidesroa.org"
  readonly dumpUrl = `${this.rootUrl}/dump`
  readonly updateUrl = `${this.rootUrl}/change_user_data`

  async putBadgeData(badge_id: string, parts: string[]): Promise<dumpResponse|errorResponse> {
    const payload:dumpMessage = {
      badge_id: badge_id,
      parts: parts
    }
    const data = await fetch(this.dumpUrl, {
                             method: 'POST',
                             body: JSON.stringify(payload)})
    return await data.json()
  }

  async updateUser(badge_id: string, token: string, username: string, teamId?: string) {
    const payload:updateUserMessage = {
      token: token,
      badge_id: badge_id,
      username: username,
      teamId: teamId
    }
    const data = await fetch(this.updateUrl, {
                             method: 'POST',
                             body: JSON.stringify(payload)})
    return await data.json()
  }
}

interface dumpMessage {
  badge_id: string
  parts: string[]
}

export interface dumpResponse {
  username: string
  teamId: string
  scoreOld: number
  scoreNew:  number
  token: string
}

export interface updateUserMessage {
  token: string
  badge_id: string
  username: string
  teamId?: string
}

interface errorResponse {
  error: string
}

interface messageResponse {
  message: string
}