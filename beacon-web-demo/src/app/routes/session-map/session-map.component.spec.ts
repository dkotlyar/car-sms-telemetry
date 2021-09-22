import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SessionMapComponent } from './session-map.component';

describe('SessionMapComponent', () => {
  let component: SessionMapComponent;
  let fixture: ComponentFixture<SessionMapComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SessionMapComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SessionMapComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
