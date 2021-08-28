from django.urls import path
from . import views

urlpatterns = [
    path('check_new_records', views.check_new_records),
    path('get_unpublished_snapshots', views.get_unpublished_snapshots),
    path('set_snapshots_statuses', views.set_snapshots_statuses),
    path('get_unpublished_media', views.get_unpublished_media),
    path('get_media_file/<int:id>', views.get_media_file),

    path('get_test', views.get_test),
    path('post_test', views.post_test)
]
