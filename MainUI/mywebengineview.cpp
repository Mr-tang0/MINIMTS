#include "mywebengineview.h"

myWebEngineView::myWebEngineView() {
    connect(this->page(), &QWebEnginePage::linkHovered, this, &myWebEngineView::slot_LinkHovered);
}
