import { Component, OnInit } from '@angular/core';
import {DjangoBackendService} from "../../services/django-backend.service";
import {Session} from "../../classes/DAO/session";
import {removeSummaryDuplicates} from "@angular/compiler";

@Component({
  selector: 'app-session-list',
  templateUrl: './session-list.component.html',
  styleUrls: ['./session-list.component.css']
})
export class SessionListComponent implements OnInit {

  sessions: Session[] = [];

  constructor(private backend: DjangoBackendService) { }

  ngOnInit(): void {
    this.backend.sessions().subscribe(result => {
      this.sessions = result.sessions;
    }, error => {
      console.error(error);
    });
  }

}
