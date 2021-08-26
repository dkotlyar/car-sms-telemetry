from django.urls import path
from . import views

urlpatterns = [
    path('get_unpublished_snapshots', views.get_unpublished_snapshots),
    path('set_snapshots_statuses', views.set_snapshots_statuses),

    path('get_test', views.get_test),
    path('post_test', views.post_test)
]
