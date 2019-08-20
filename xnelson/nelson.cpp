#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/display.h>

#include <ctype.h>
#include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/font.h>

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

wxDEFINE_EVENT(EVT_CHANGE_DIRECTORY, wxCommandEvent);

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

class NelsonWindow : public wxPanel {
private:
  int _selected;
  std::unique_ptr<wxString> _cached_selection;
  wxString _query;
  std::unique_ptr<wxDir> _cwd;
  std::vector<wxString> _entries;

  void OnPaint(wxPaintEvent &evt);
  void OnChangeDirectory(wxCommandEvent &evt);
  void OnCharacterTyped(wxKeyEvent &evt);

  void Render(wxDC &dc);
  void Select();
  void ChangeDirectory(wxString destination);
  void Plumb(wxString destination);

public:
  NelsonWindow(wxFrame *parent);
};

NelsonWindow::NelsonWindow(wxFrame *parent)
    : wxPanel(parent), _selected(0), _cached_selection(nullptr), _query(""),
      _cwd(std::make_unique<wxDir>(wxGetCwd())), _entries{} {

  // Setup event binds.
  Bind(EVT_CHANGE_DIRECTORY, &NelsonWindow::OnChangeDirectory, this);
  Bind(wxEVT_PAINT, &NelsonWindow::OnPaint, this);
  Bind(wxEVT_CHAR, &NelsonWindow::OnCharacterTyped, this);

  // Post the cd event which we need in order to populate the initial list.
  wxCommandEvent change_direcotory_event(EVT_CHANGE_DIRECTORY);
  wxPostEvent(this, change_direcotory_event);
}

void NelsonWindow::OnPaint(wxPaintEvent &evt) {
  wxPaintDC dc(this);
  Render(dc);
}

void NelsonWindow::OnChangeDirectory(wxCommandEvent &evt) {
  _entries.clear();
  _query.clear();
  _cached_selection = nullptr;
  _selected = 0;

  wxString filename;
  bool cont = _cwd->GetFirst(&filename);
  _entries.push_back(filename);
  while (cont) {
    cont = _cwd->GetNext(&filename);
    if (cont) {
      _entries.push_back(filename);
    }
  }
  Refresh();
}

void NelsonWindow::Plumb(wxString destination) {
  const char *cpath = destination.c_str();
  int err = execlp("plumb", "-dedit", cpath, nullptr);
  if (err) {
    fprintf(stderr, "plumb -d edit %s: ", cpath);
    perror(nullptr);
    fprintf(stderr, "\n");
  }
}

void NelsonWindow::ChangeDirectory(wxString destination) {
  if (!wxSetWorkingDirectory(destination)) {
    std::cerr << "Failed to cd to: " << destination.c_str() << '\n';
  } else {
    _cwd = std::make_unique<wxDir>(wxGetCwd());
    wxCommandEvent change_direcotory_event(EVT_CHANGE_DIRECTORY);
    wxPostEvent(this, change_direcotory_event);
  }
}

void NelsonWindow::Select() {
  // If there's a cached selection, append it to the cwd. Otherwise, append
  // the query to the cwd. This constitutes the destination.
  wxString destination;
  if (_selected < 0) {
    // TODO(esp): A bit janky that we have to test the selected index in order
    // to know whether we can trust the cached selection.
    destination = _cwd->GetNameWithSep().Append(_query);
  } else {
    destination = _cwd->GetNameWithSep().Append(*_cached_selection);
  }

  if (wxDirExists(destination)) {
    // If the destination is a directory that exists, set our cwd to that
    // directory.

    ChangeDirectory(destination);
  } else if (wxFileExists(destination)) {
    // If the destination is a file that exists, go ahead and plumb the file.

    Plumb(destination);
  } else {
    // The destination doesn't exist, so we to create any missing directories
    // in the path, then the file component (if provided).

    wxFileName destination1(destination);

    // Walk backwards til we find a directory that exists.
    wxFileName destination2(destination);
    std::vector<wxString> directory_stack;
    while (!destination2.DirExists()) {
      directory_stack.push_back(destination2.GetPathWithSep());
      destination2.RemoveLastDir();
    }

    // Pop off the stack of uncreated directories, creating as we go.
    while (directory_stack.size() > 0) {
      wxFileName to_mkdir(directory_stack.back());
      if (to_mkdir.Mkdir(wxS_DIR_DEFAULT | wxPATH_MKDIR_FULL)) {
        directory_stack.pop_back();
      } else {
        std::cerr << "Failed to create: " << to_mkdir.GetPathWithSep() << '\n';
        return;
      }
    }

    if (destination1.HasName()) {
      wxFile f(destination1.GetFullPath(), wxFile::write_excl);
      if (!f.IsOpened()) {
        std::cerr << "Failed to open file: " << destination1.GetFullPath()
                  << '\n';
        return;
      }
      Plumb(destination);
    } else {
      ChangeDirectory(destination);
    }
  }
}

void NelsonWindow::OnCharacterTyped(wxKeyEvent &evt) {
  wxChar uc = evt.GetUnicodeKey();
  if (uc != WXK_NONE) {
    switch (uc) {
    case WXK_RETURN: {
      Select();
    }; break;
    case WXK_TAB: {
      // Tab is like a forward arrow.
      if (_selected < (static_cast<int>(_entries.size()) - 1)) {
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
      // If there's an active query, backspace from it. Otherwise, cd up a
      // level.
      if (_query.size() > 0) {
        _query.RemoveLast();
      } else {
        ChangeDirectory(_cwd->GetNameWithSep().Append(wxT("..")));
      }
    }; break;

    //
    // Specific shortcut handling
    //

    case 'R': {
      if (evt.RawControlDown()) {
        break;
      }
    } // Fallthru
    case 'W': {
      if (evt.RawControlDown()) {
        // For Control-W, nuke the query entirely if there is one. Otherwise, go
        // up a directory.
        if (_query.size() > 0) {
          _query.Remove(0);
        } else {
          ChangeDirectory(_cwd->GetNameWithSep().Append(wxT("..")));
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
        _selected = static_cast<int>(_entries.size()) - 1;
      }
    }; break;

    case WXK_RIGHT:
    case WXK_DOWN: {
      if (_selected < (static_cast<int>(_entries.size()) - 1)) {
        ++_selected;
      } else {
        _selected = 0;
      }
    }; break;
    }
  }
  Refresh();
}

void NelsonWindow::Render(wxDC &dc) {
  wxRect rc = GetClientRect();

  wxFont normal_font(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                     wxFONTWEIGHT_NORMAL);
  wxFont bold_font(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                   wxFONTWEIGHT_BOLD);

  std::vector<std::pair<int, wxString>> filtered_entries;

  // Cache the filtered entries.
  for (auto &entry : _entries) {
    int score = 0;
    bool matched = score_entry(_query.Len(), _query.c_str(), entry.Len(),
                               entry.c_str(), &score);
    if (!matched) {
      continue;
    } else {
      filtered_entries.push_back({score, entry});
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
  auto cwd = _cwd->GetNameWithSep();
  auto cwd_size = dc.GetTextExtent(cwd);
  dc.DrawText(cwd, rc.GetLeft(), rc.GetTop());
  auto query_size = dc.GetTextExtent(_query);
  dc.DrawText(_query, rc.GetLeft() + cwd_size.GetWidth(), rc.GetTop());
  running_elevation += cwd_size.GetHeight();

  // Render each result entry
  for (int i = 0; i < filtered_entries_count; ++i) {
    auto &entry = filtered_entries[i].second;

    auto text_size = dc.GetTextExtent(entry);

    // Draw the selected entry in bold
    if (i == _selected) {
      dc.SetFont(bold_font);
      _cached_selection = std::make_unique<wxString>(entry);
    } else {
      dc.SetFont(normal_font);
    }

    dc.DrawText(entry, rc.GetLeft(), rc.GetTop() + running_elevation);
    running_elevation += text_size.GetHeight();
  }
}

class NelsonFrame : public wxFrame {
private:
  wxBoxSizer *_entry_box;

public:
  NelsonFrame(const wxString &title, const wxPoint &pos, const wxSize &size,
              long style);
};

NelsonFrame::NelsonFrame(const wxString &title, const wxPoint &pos,
                         const wxSize &size, long style)
    : wxFrame(nullptr, wxID_ANY, title, pos, size, style),
      _entry_box(new wxBoxSizer(wxVERTICAL)) {
  unsigned int display_idx = wxDisplay::GetFromWindow(this);
  wxDisplay display{display_idx};
  wxRect screen = display.GetClientArea();
  int width = screen.width / 2;
  int height = screen.height / 2;
  int x = (screen.width - width) / 2;
  int y = (screen.height - height) / 2;
  wxRect nelson_rect = wxRect(x, y, width, height);
  _entry_box->Add(new NelsonWindow(this), 1, wxEXPAND);
  SetSizer(_entry_box);
  SetSize(nelson_rect);
}

class MyApp : public wxApp {
private:
  NelsonFrame *_nelson_frame;

public:
  virtual bool OnInit();
};

bool MyApp::OnInit() {
  _nelson_frame = new NelsonFrame("Nelson", wxPoint(-1, -1), wxSize(-1, -1),
                                  wxDEFAULT_FRAME_STYLE);
  _nelson_frame->Show(true);
  return true;
};

wxIMPLEMENT_APP(MyApp);
