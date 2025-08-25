// client_gui.c : Minimal GTK4 UI wrapping the existing C server protocol
#include "ft_core.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GtkWidget *win;
  GtkEntry  *host;
  GtkEntry  *port;
  GtkButton *btn_connect;
  GtkButton *btn_upload;
  GtkLabel  *status;
  GtkTreeView *remote_view;
  GtkListStore *remote_store;
  int sock;
} App;

static void set_status(App* a, const char* fmt, ...){
  char buf[512];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  gtk_label_set_text(GTK_LABEL(a->status), buf);
}

static void remote_store_clear(App* a){
  if (!a->remote_store) return;
  gtk_list_store_clear(a->remote_store);
}

static void refresh_remote(App* a){
  if (a->sock < 0) return;
  char **names = NULL; int cnt = 0;
  if (ft_sls(a->sock, &names, &cnt) != 0){ set_status(a, "sls failed"); return; }
  if (!a->remote_store) a->remote_store = gtk_list_store_new(1, G_TYPE_STRING);
  else remote_store_clear(a);
  for (int i=0;i<cnt;i++){
    GtkTreeIter it; gtk_list_store_append(a->remote_store, &it);
    gtk_list_store_set(a->remote_store, &it, 0, names[i], -1);
  }
  gtk_tree_view_set_model(a->remote_view, GTK_TREE_MODEL(a->remote_store));
  ft_sls_free(names, cnt);
}

static void on_connect(GtkButton* b, gpointer user){
  App* a = user;
  const char* h = gtk_editable_get_text(GTK_EDITABLE(a->host));
  const char* p = gtk_editable_get_text(GTK_EDITABLE(a->port));
  int port = atoi(p?p:"0");
  a->sock = ft_connect(h, port);
  if (a->sock < 0){ set_status(a, "Bağlantı başarısız"); return; }
  char cwd[1024];
  if (ft_spwd(a->sock, cwd, sizeof(cwd))==0){
    set_status(a, "Bağlandı: %s:%d  cwd=%s", h, port, cwd);
  } else set_status(a, "Bağlandı: %s:%d", h, port);
  refresh_remote(a);
}

static void on_upload(GtkButton* b, gpointer user){
  App* a = user;
  if (a->sock < 0){ set_status(a, "Önce bağlan"); return; }
  GtkFileChooserNative *dlg = gtk_file_chooser_native_new("Dosya seç", GTK_WINDOW(a->win), GTK_FILE_CHOOSER_ACTION_OPEN, "Seç", "İptal");
  if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT){
    GtkFileChooser *fc = GTK_FILE_CHOOSER(dlg);
    char *path = gtk_file_chooser_get_filename(fc);
    if (path){
      int r = ft_put_file(a->sock, path, "."); // gönder ve aynı dizine kaydet
      if (r==0){ set_status(a, "Yüklendi: %s", ft_basename(path)); refresh_remote(a); }
      else set_status(a, "Yükleme hatası");
      g_free(path);
    }
  }
  g_object_unref(dlg);
}

static GtkWidget* make_remote_view(App* a){
  GtkWidget *view = gtk_tree_view_new();
  GtkCellRenderer *r = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes("Server", r, "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
  a->remote_view = GTK_TREE_VIEW(view);
  return view;
}

int main(int argc, char** argv){
  gtk_init();

  App a = {0}; a.sock = -1;

  a.win = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(a.win), "FTP-like Client (GTK4/C)");
  gtk_window_set_default_size(GTK_WINDOW(a.win), 900, 500);

  GtkWidget *host = gtk_entry_new(); a.host = GTK_ENTRY(host);
  gtk_editable_set_text(GTK_EDITABLE(host), "127.0.0.1");
  GtkWidget *port = gtk_entry_new(); a.port = GTK_ENTRY(port);
  gtk_editable_set_text(GTK_EDITABLE(port), "5000");
  GtkWidget *btnc = gtk_button_new_with_label("Connect"); a.btn_connect = GTK_BUTTON(btnc);
  GtkWidget *btnu = gtk_button_new_with_label("Upload");  a.btn_upload  = GTK_BUTTON(btnu);
  GtkWidget *status = gtk_label_new("Hazır"); a.status = GTK_LABEL(status);

  g_signal_connect(btnc, "clicked", G_CALLBACK(on_connect), &a);
  g_signal_connect(btnu, "clicked", G_CALLBACK(on_upload), &a);

  GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(hdr), gtk_label_new("Host")); gtk_box_append(GTK_BOX(hdr), host);
  gtk_box_append(GTK_BOX(hdr), gtk_label_new("Port")); gtk_box_append(GTK_BOX(hdr), port);
  gtk_box_append(GTK_BOX(hdr), btnc); gtk_box_append(GTK_BOX(hdr), btnu);

  GtkWidget *view = make_remote_view(&a);

  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append(GTK_BOX(root), hdr);
  gtk_box_append(GTK_BOX(root), view);
  gtk_box_append(GTK_BOX(root), status);
  gtk_window_set_child(GTK_WINDOW(a.win), root);

  gtk_window_present(GTK_WINDOW(a.win));
  gtk_main();
  return 0;
}
