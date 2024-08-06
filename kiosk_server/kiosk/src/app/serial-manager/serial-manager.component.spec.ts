// biome-ignore lint/style/useImportType: <explanation>
import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SerialManagerComponent } from './serial-manager.component';
import {MatExpansionModule} from '@angular/material/expansion';


describe('SerialManagerComponent', () => {
  let component: SerialManagerComponent;
  let fixture: ComponentFixture<SerialManagerComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SerialManagerComponent, MatExpansionModule]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(SerialManagerComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
