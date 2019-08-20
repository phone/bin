#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/display.h>

#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <vector>

#define MAXLINES 100000

bool score_entry(int query_sz, const char *query, int entry_sz,
                 const char *entry, int *score_out) {
  int score = 0;
  int gap = 0;
  int qdx = 0;
  int edx = 0;
  bool prev_matched = false;

  while (qdx < query_sz && edx < entry_sz) {
    if (tolower(query[qdx]) == tolower(entry[edx])) {
      ++score;
      if (query[qdx] == entry[edx]) {
        // If we match in a case-sensitive comparison, bump the score further.
        ++score;
      }
      if (edx == 0 && qdx == 0) {
        // If the first character matches, that's a very strong indicator.
        score += 16;
      } else {
        // We hit this block every iteration except the first one.
        if ((isupper(entry[edx]) && islower(entry[edx - 1])) ||
            (entry[edx - 1] == '_') || (entry[edx - 1] == '-') ||
            (entry[edx - 1] == ' ')) {
          // If we hit the first character of a camelcase entry, or some other
          // situation where we seem to be at the beginning of a new word.
          score += 8;
        }
        if (edx > 1 && entry[edx - 1] == '.') {
          // We should bump the score if it seems like we might be matching an
          // extension, but don't trigger it on hidden files, where the first
          // character is a dot.
          score += 4;
        }
      }
      if (prev_matched) {
        // Slight score bump for consecutive matches.
        ++score;
      }

      // If we found a match, bump the query index.
      ++qdx;
      prev_matched = true;
    } else {
      // No match, bump the gap penalty.
      ++gap;
      prev_matched = false;
    }
    // Regardless of match, bump the entry index.
    ++edx;
  }

  // Subtract gap from the score
  score -= gap;
  *score_out = score;

  // Return true if we got thru the entire query, false otherwise.
  return qdx == query_sz;
}

class XchooseWindow : public wxScrolledWindow {
public:
  int _selected;
  wxString _cached_selection;
  wxString _query;
  std::vector<std::pair<int, char *>> lines;

  XchooseWindow(wxFrame *parent);
  void OnPaint(wxPaintEvent &evt);
  void OnCharacterTyped(wxKeyEvent &evt);
  void Render(wxDC &dc);
  void Select();
};

XchooseWindow::XchooseWindow(wxFrame *parent)
    : wxScrolledWindow(parent),
      _selected(0), _cached_selection{}, _query{}, lines{} {
  char *line = {};
  size_t linecap = 0;
  ssize_t linelen = 0;
  while (lines.size() < MAXLINES &&
         (linelen = getline(&line, &linecap, stdin)) > 0) {
    lines.push_back({linelen, strndup(line, linelen)});
  }
  Bind(wxEVT_PAINT, &XchooseWindow::OnPaint, this);
  Bind(wxEVT_CHAR, &XchooseWindow::OnCharacterTyped, this);
}

void XchooseWindow::OnPaint(wxPaintEvent &evt) {
  wxPaintDC dc(this);
  Render(dc);
}

void XchooseWindow::Select() {
  // Basically, just print the selected entry and exit immediately.
  printf("%s", (char *)_cached_selection.char_str());
  exit(0);
}

void XchooseWindow::OnCharacterTyped(wxKeyEvent &evt) {
  wxChar uc = evt.GetUnicodeKey();
  if (uc != WXK_NONE) {
    switch (uc) {
    case WXK_RETURN: {
      Select();
    }; break;
    case WXK_TAB: {
      // Tab is like a forward arrow.
      if (_selected < (static_cast<int>(lines.size()) - 1)) {
        ++_selected;
      } else {
        _selected = 0;
      }
    }; break;
    case WXK_ESCAPE: {
      exit(0);
    }; break;
    case WXK_DELETE:
    case WXK_BACK: {
      // If there's an active query, backspace from it.
      if (_query.size() > 0) {
        _query.RemoveLast();
      }
    }; break;

    //
    // Specific shortcut handling
    //

    case 'W': {
      if (evt.RawControlDown()) {
        // For Control-W, nuke the query entirely if there is one.
        if (_query.size() > 0) {
          _query.Remove(0);
        }
        break;
      }
    } // Fallthru
    default: { _query.Append(uc); }; break;
    }

  } else {
    switch (evt.GetKeyCode()) {
    case WXK_LEFT:
    case WXK_UP: {
      if (_selected > 0) {
        --_selected;
      } else {
        _selected = static_cast<int>(lines.size()) - 1;
      }
    }; break;

    case WXK_RIGHT:
    case WXK_DOWN: {
      if (_selected < (static_cast<int>(lines.size()) - 1)) {
        ++_selected;
      } else {
        _selected = 0;
      }
    }; break;
    }
  }
  Refresh();
}

void XchooseWindow::Render(wxDC &dc) {
  wxRect rc = GetClientRect();

  wxFont normal_font(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                     wxFONTWEIGHT_NORMAL);
  wxFont bold_font(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                   wxFONTWEIGHT_BOLD);

  std::vector<std::pair<int, wxString>> filtered_entries;

  // Cache the filtered entries.
  for (auto &line_pair : lines) {
    int line_len = line_pair.first;
    char *line = line_pair.second;
    int score = 0;
    bool matched =
        score_entry(_query.Len(), _query.c_str(), line_len, line, &score);
    if (!matched) {
      continue;
    } else {
      // TODO(esp): set it up so these don't copy every frame
      filtered_entries.push_back({score, wxString(line_pair.second)});
    }
  }
  std::sort(filtered_entries.begin(), filtered_entries.end(),
            [](const std::pair<int, wxString> &a,
               const std::pair<int, wxString> &b) -> bool {
              return a.first > b.first;
            });
  auto filtered_entries_count = static_cast<int>(filtered_entries.size());

  // If we selected an entry that's filtered out, just set it to the highest
  // index we can.
  if (_selected < 0) {
    _selected = 0;
  }
  if (_selected > filtered_entries_count - 1) {
    _selected = filtered_entries_count - 1;
  }

  // If there are no entries to select, bold the query.
  if (filtered_entries_count == 0) {
    dc.SetFont(bold_font);
    dc.SetTextForeground(*wxBLACK);
  } else {
    dc.SetFont(normal_font);
    dc.SetTextForeground(*wxBLACK);
  }

  int running_elevation = 0;

  // Draw the query line
  if (_query.size()) {
    auto query_size = dc.GetTextExtent(_query);
    dc.DrawText(_query, rc.GetLeft(), rc.GetTop());
    running_elevation += query_size.GetHeight();
  } else {
    auto fake_news = dc.GetTextExtent(wxT("fake"));
    running_elevation += fake_news.GetHeight();
  }

  // Render each result entry
  for (int i = 0; i < filtered_entries_count; ++i) {
    auto &entry = filtered_entries[i].second;

    auto text_size = dc.GetTextExtent(entry);

    // Draw the selected entry in bold
    if (i == _selected) {
      dc.SetFont(bold_font);
      _cached_selection = entry;
    } else {
      dc.SetFont(normal_font);
    }

    dc.DrawText(entry, rc.GetLeft(), rc.GetTop() + running_elevation);
    running_elevation += text_size.GetHeight();
  }
}

class XchooseFrame : public wxFrame {
public:
  wxBoxSizer *entry_box;

  XchooseFrame(const wxString &title, const wxPoint &pos, const wxSize &size,
               long style);
};

XchooseFrame::XchooseFrame(const wxString &title, const wxPoint &pos,
                           const wxSize &size, long style)
    : wxFrame(nullptr, wxID_ANY, title, pos, size, style),
      entry_box(new wxBoxSizer(wxVERTICAL)) {
  unsigned int display_idx = wxDisplay::GetFromWindow(this);
  wxDisplay display{display_idx};
  wxRect screen = display.GetClientArea();
  int width = 3 * (screen.width / 4);
  int height = 3 * (screen.height / 4);
  int x = (screen.width - width) / 2;
  int y = (screen.height - height) / 2;
  wxRect xchoose_rect = wxRect(x, y, width, height);
  entry_box->Add(new XchooseWindow(this), 1, wxEXPAND);
  SetSizer(entry_box);
  SetSize(xchoose_rect);
}

class MyApp : public wxApp {
private:
  XchooseFrame *_nelson_frame;

public:
  virtual bool OnInit();
};

bool MyApp::OnInit() {
  auto _xchoose_frame = new XchooseFrame("Xchoose", wxPoint(-1, -1),
                                         wxSize(-1, -1), wxDEFAULT_FRAME_STYLE);
  _xchoose_frame->Show(true);
  return true;
};

wxIMPLEMENT_APP(MyApp);
