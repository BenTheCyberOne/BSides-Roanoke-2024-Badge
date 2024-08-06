// biome-ignore lint/style/useImportType: <explanation>
import { ComponentFixture, TestBed } from '@angular/core/testing';

import { DynamicDropdownComponent } from './dynamic-dropdown.component';

describe('DynamicDropdownComponent', () => {
  let component: DynamicDropdownComponent;
  let fixture: ComponentFixture<DynamicDropdownComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [DynamicDropdownComponent]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(DynamicDropdownComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
