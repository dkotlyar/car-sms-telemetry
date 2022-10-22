import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';

import { AppComponent } from './app.component';
import {HttpClientModule} from "@angular/common/http";
import {RouterModule, Routes} from "@angular/router";
import { SessionMapComponent } from './routes/session-map/session-map.component';
import { SessionListComponent } from './routes/session-list/session-list.component';

const appRoutes: Routes = [
  {path: '', component: SessionListComponent},
  {path: ':id', component: SessionMapComponent},
];

@NgModule({
  declarations: [
    AppComponent,
    SessionMapComponent,
    SessionListComponent
  ],
  imports: [
    BrowserModule,
    HttpClientModule,
    RouterModule.forRoot(appRoutes)
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
