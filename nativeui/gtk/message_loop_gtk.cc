// Copyright 2018 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#include "nativeui/message_loop.h"

#include <gtk/gtk.h>

#include "nativeui/gtk/widget_util.h"

namespace nu {

namespace {

gboolean OnSource(std::function<void()>* func) {
  (*func)();
  return G_SOURCE_REMOVE;
}

}  // namespace

// static
void MessageLoop::Run() {
  gtk_main();
}

// static
void MessageLoop::Quit() {
  gtk_main_quit();
}

// static
void MessageLoop::PostTask(const std::function<void()>& task) {
  g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(OnSource),
                  new Task(task), Delete<Task>);
}

// static
void MessageLoop::PostDelayedTask(int ms, const std::function<void()>& task) {
  g_timeout_add_full(G_PRIORITY_DEFAULT, ms,
                     reinterpret_cast<GSourceFunc>(OnSource),
                     new Task(task), Delete<Task>);
}

}  // namespace nu
