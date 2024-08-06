import { TestBed } from '@angular/core/testing';

import { BadgeApiService } from './badge-api.service';

describe('BadgeApiService', () => {
  let service: BadgeApiService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(BadgeApiService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
