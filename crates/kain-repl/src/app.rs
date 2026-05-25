use arboard::Clipboard;
use kain_core::tooling_config::normalize_ui_theme_name;
use std::io;
use std::time::Duration;

use crossterm::event::{self, Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use crossterm::execute;
use crossterm::terminal::{
    disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use ratatui::backend::CrosstermBackend;
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::style::{Modifier, Style};
use ratatui::text::{Line, Span, Text};
use ratatui::widgets::{Block, Borders, Clear, Paragraph};
use ratatui::{Frame, Terminal};
use unicode_width::UnicodeWidthStr;

use crate::command::{parse_theme_argument, ReplDirective, REPL_HELP_TEXT};
use crate::evaluation::{ReplEvaluation, ReplEvaluator};
use crate::highlight::highlight_source_line;
use crate::source::normalize_script_source;
use crate::terminal::ReplTerminalConfig;
use crate::theme::{active_repl_theme_name, repl_palette, repl_theme_names, ReplPalette};

const TAB_WIDTH: usize = 4;
const OUTPUT_HISTORY_LIMIT: usize = 64;

pub fn run_tui_repl(config: ReplTerminalConfig) -> io::Result<()> {
    let mut stdout = io::stdout();
    enable_raw_mode()?;
    execute!(stdout, EnterAlternateScreen)?;

    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;
    terminal.clear()?;

    let mut app = ReplApp::new(config);
    let app_result = run_event_loop(&mut terminal, &mut app);

    let cleanup_result = restore_terminal(&mut terminal);
    match (app_result, cleanup_result) {
        (Ok(()), Ok(())) => Ok(()),
        (Err(err), Ok(())) => Err(err),
        (Ok(()), Err(err)) => Err(err),
        (Err(err), Err(_cleanup_err)) => Err(err),
    }
}

fn restore_terminal(terminal: &mut Terminal<CrosstermBackend<io::Stdout>>) -> io::Result<()> {
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()?;
    Ok(())
}

fn run_event_loop(
    terminal: &mut Terminal<CrosstermBackend<io::Stdout>>,
    app: &mut ReplApp,
) -> io::Result<()> {
    loop {
        terminal.draw(|frame| app.render(frame))?;
        if app.should_quit {
            return Ok(());
        }

        if !event::poll(Duration::from_millis(50))? {
            continue;
        }

        match event::read()? {
            Event::Key(key) if key.kind == KeyEventKind::Press => app.handle_key(key),
            Event::Paste(content) => app.editor.insert_text(&content),
            Event::Resize(_, _) => {}
            _ => {}
        }
    }
}

#[derive(Debug, Clone)]
struct ReplApp {
    config: ReplTerminalConfig,
    evaluator: ReplEvaluator,
    editor: ReplEditor,
    theme_name: String,
    output_log: Vec<OutputEntry>,
    show_help: bool,
    should_quit: bool,
    next_run_id: usize,
    status: String,
}

impl ReplApp {
    fn new(config: ReplTerminalConfig) -> Self {
        let mut app = Self {
            config,
            evaluator: ReplEvaluator::default(),
            editor: ReplEditor::default(),
            theme_name: active_repl_theme_name(),
            output_log: Vec::new(),
            show_help: false,
            should_quit: false,
            next_run_id: 0,
            status: "Ready. Ctrl+Enter or F5 runs the current buffer.".to_string(),
        };
        app.push_output(
            OutputKind::Info,
            "Session opened",
            vec![
                "Multiline source stays live between runs so you can patch and rerun quickly."
                    .to_string(),
            ],
        );
        app
    }

    fn handle_key(&mut self, key: KeyEvent) {
        match key {
            KeyEvent {
                code: KeyCode::Char('q'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.should_quit = true;
            }
            KeyEvent {
                code: KeyCode::Char('c'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.copy_buffer_to_clipboard(),
            KeyEvent {
                code: KeyCode::Char('v'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.paste_from_clipboard(),
            KeyEvent {
                code: KeyCode::Enter,
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.run_current_buffer();
            }
            KeyEvent {
                code: KeyCode::F(5),
                ..
            } => self.run_current_buffer(),
            KeyEvent {
                code: KeyCode::F(1),
                ..
            } => self.show_help = !self.show_help,
            KeyEvent {
                code: KeyCode::Char('l'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.editor.clear();
                self.status = "Input buffer cleared.".to_string();
            }
            KeyEvent {
                code: KeyCode::Char('k'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.output_log.clear();
                self.status = "Output history wiped clean.".to_string();
            }
            KeyEvent {
                code: KeyCode::Esc, ..
            } => self.show_help = false,
            KeyEvent {
                code: KeyCode::Left,
                ..
            } => self.editor.move_left(),
            KeyEvent {
                code: KeyCode::Right,
                ..
            } => self.editor.move_right(),
            KeyEvent {
                code: KeyCode::Up, ..
            } => self.editor.move_up(),
            KeyEvent {
                code: KeyCode::Down,
                ..
            } => self.editor.move_down(),
            KeyEvent {
                code: KeyCode::Home,
                ..
            } => self.editor.move_home(),
            KeyEvent {
                code: KeyCode::End, ..
            } => self.editor.move_end(),
            KeyEvent {
                code: KeyCode::PageUp,
                ..
            } => self.editor.page_up(),
            KeyEvent {
                code: KeyCode::PageDown,
                ..
            } => self.editor.page_down(),
            KeyEvent {
                code: KeyCode::Backspace,
                ..
            } => self.editor.backspace(),
            KeyEvent {
                code: KeyCode::Delete,
                ..
            } => self.editor.delete(),
            KeyEvent {
                code: KeyCode::Tab, ..
            } => self.editor.insert_spaces(TAB_WIDTH),
            KeyEvent {
                code: KeyCode::Enter,
                ..
            } => self.editor.insert_newline(),
            KeyEvent {
                code: KeyCode::Char(ch),
                modifiers,
                ..
            } if !modifiers.contains(KeyModifiers::CONTROL)
                && !modifiers.contains(KeyModifiers::ALT) =>
            {
                self.editor.insert_char(ch);
            }
            _ => {}
        }
    }

    fn run_current_buffer(&mut self) {
        match collect_submission(&self.editor.buffer()) {
            Submission::Empty => {
                self.status = "Buffer is empty. Feed it some Kain.".to_string();
            }
            Submission::Help => {
                self.show_help = true;
                self.status = "Help overlay opened.".to_string();
            }
            Submission::Clear => {
                self.editor.clear();
                self.status = "Input buffer cleared.".to_string();
            }
            Submission::Theme(theme) => self.apply_theme_command(theme),
            Submission::Exit => {
                self.should_quit = true;
            }
            Submission::Evaluate {
                source,
                stripped_run_directive,
            } => {
                if stripped_run_directive {
                    self.editor.set_text(&source);
                }
                self.evaluate_source(source);
            }
        }
    }

    fn evaluate_source(&mut self, source: String) {
        self.next_run_id += 1;
        let run_id = self.next_run_id;
        let line_count = source.lines().count().max(1);
        let headline = format!(
            "Run #{run_id} · {line_count} lines · {}",
            preview_source_headline(&source)
        );

        match self
            .evaluator
            .evaluate_interpret_source(&self.config.source_name, &source)
        {
            Ok(evaluation) => {
                self.push_output(OutputKind::Success, headline, evaluation_lines(&evaluation));
                self.editor.clear();
                self.status =
                    format!("Run #{run_id} landed clean. Buffer reset for the next swing.");
            }
            Err(error) => {
                let body = error
                    .plain_text()
                    .lines()
                    .map(|line| line.to_string())
                    .collect::<Vec<_>>();
                self.push_output(OutputKind::Error, headline, body);
                self.status = format!(
                    "Run #{run_id} hit diagnostics. Buffer stayed live so you can patch and rerun."
                );
            }
        }
    }

    fn push_output(&mut self, kind: OutputKind, title: impl Into<String>, body: Vec<String>) {
        self.output_log.push(OutputEntry {
            kind,
            title: title.into(),
            body,
        });
        if self.output_log.len() > OUTPUT_HISTORY_LIMIT {
            let drain_count = self.output_log.len() - OUTPUT_HISTORY_LIMIT;
            self.output_log.drain(0..drain_count);
        }
    }

    fn render(&mut self, frame: &mut Frame<'_>) {
        let area = frame.area();
        let palette = repl_palette(&self.theme_name);
        let layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([
                Constraint::Length(3),
                Constraint::Min(10),
                Constraint::Length(1),
            ])
            .split(area);

        self.render_header(frame, layout[0], palette);
        self.render_body(frame, layout[1], palette);
        self.render_status(frame, layout[2], palette);

        if self.show_help {
            self.render_help_overlay(frame, area, palette);
        }
    }

    fn render_header(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let banner = Paragraph::new(Text::from(vec![
            Line::from(vec![
                Span::styled(
                    " Kain REPL Workbench ",
                    palette.title_style(),
                ),
                Span::styled(
                    self.config.metadata.banner(),
                    Style::default().fg(palette.chrome_secondary),
                ),
            ]),
            Line::from(vec![Span::styled(
                format!(
                    " Multiline Kain composer with {} theme, persistent buffer, and in-session diagnostics.",
                    self.theme_name
                ),
                Style::default().fg(palette.text_muted),
            )]),
        ]))
        .block(
            Block::default()
                .borders(Borders::ALL)
                .border_style(Style::default().fg(palette.border_focus))
                .style(Style::default().bg(palette.panel_background)),
        );
        frame.render_widget(banner, area);
    }

    fn render_body(&mut self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let panels = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Min(8), Constraint::Length(10)])
            .split(area);
        self.render_editor(frame, panels[0], palette);
        self.render_output(frame, panels[1], palette);
    }

    fn render_editor(&mut self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let title = format!(
            " Editor · {} lines · {} chars ",
            self.editor.line_count(),
            self.editor.char_count()
        );
        let block = Block::default()
            .title(title)
            .borders(Borders::ALL)
            .border_style(Style::default().fg(palette.border_focus))
            .style(Style::default().bg(palette.panel_background));
        let inner = block.inner(area);
        frame.render_widget(block, area);

        if inner.width < 3 || inner.height < 1 {
            return;
        }

        let gutter_width = self.editor.gutter_width();
        let columns = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Length(gutter_width as u16), Constraint::Min(1)])
            .split(inner);

        self.editor
            .ensure_visible(columns[1].width as usize, columns[1].height as usize);

        let line_numbers = self.editor.render_line_numbers(palette);
        let source_lines = self.editor.render_source_lines(palette);

        frame.render_widget(
            Paragraph::new(line_numbers)
                .scroll((self.editor.scroll_y as u16, 0))
                .style(Style::default().bg(palette.panel_background)),
            columns[0],
        );
        frame.render_widget(
            Paragraph::new(source_lines)
                .scroll((self.editor.scroll_y as u16, self.editor.scroll_x as u16))
                .style(Style::default().bg(palette.panel_background)),
            columns[1],
        );

        let (cursor_x, cursor_y) = self.editor.cursor_screen_position(columns[1]);
        frame.set_cursor_position((cursor_x, cursor_y));
    }

    fn render_output(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let block = Block::default()
            .title(format!(" Output · {} entries ", self.output_log.len()))
            .borders(Borders::ALL)
            .border_style(Style::default().fg(palette.border))
            .style(Style::default().bg(palette.panel_background));
        let inner = block.inner(area);
        frame.render_widget(block, area);

        let lines = self.output_lines(palette);
        let scroll_y = lines
            .len()
            .saturating_sub(inner.height as usize)
            .try_into()
            .unwrap_or(u16::MAX);
        frame.render_widget(
            Paragraph::new(Text::from(lines))
                .scroll((scroll_y, 0))
                .style(Style::default().bg(palette.panel_background)),
            inner,
        );
    }

    fn render_status(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let status = format!(
            " Ln {}, Col {}  |  {}  |  Ctrl+Enter/F5 run  F1 help  Ctrl+Q quit ",
            self.editor.cursor_row + 1,
            self.editor.cursor_col + 1,
            self.status
        );
        frame.render_widget(
            Paragraph::new(Line::from(Span::styled(
                status,
                Style::default()
                    .fg(palette.status_fg)
                    .bg(palette.status_bg)
                    .add_modifier(Modifier::BOLD),
            ))),
            area,
        );
    }

    fn render_help_overlay(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let popup = centered_rect(78, 70, area);
        let help_text = Text::from(vec![
            Line::from(Span::styled("Kain REPL Controls", palette.title_style())),
            Line::raw(""),
            Line::raw("Ctrl+Enter or F5  run the current buffer"),
            Line::raw("Enter               insert a newline"),
            Line::raw("Tab                 insert four spaces"),
            Line::raw("Ctrl+C / Ctrl+Shift+C copy the current buffer"),
            Line::raw("Ctrl+V / Ctrl+Shift+V paste from the system clipboard"),
            Line::raw("Ctrl+L              clear the composer buffer"),
            Line::raw("Ctrl+K              clear the output pane"),
            Line::raw("Ctrl+Q              quit the REPL"),
            Line::raw("F1 / Esc            toggle this help"),
            Line::raw(""),
            Line::raw("Dot directives still work when they are the whole buffer:"),
            Line::raw(REPL_HELP_TEXT),
            Line::raw(""),
            Line::raw("If you end the buffer with `.run`, the REPL strips that line"),
            Line::raw("before evaluation so old muscle memory still feels natural."),
        ]);

        frame.render_widget(Clear, popup);
        frame.render_widget(
            Paragraph::new(help_text).block(
                Block::default()
                    .title(" Help ")
                    .borders(Borders::ALL)
                    .border_style(Style::default().fg(palette.border_focus))
                    .style(Style::default().bg(palette.panel_background)),
            ),
            popup,
        );
    }

    fn output_lines(&self, palette: ReplPalette) -> Vec<Line<'static>> {
        if self.output_log.is_empty() {
            return vec![Line::from(Span::styled(
                "No runs yet.",
                palette.muted_style(),
            ))];
        }

        let mut lines = Vec::new();
        for entry in &self.output_log {
            lines.push(Line::from(Span::styled(
                entry.title.clone(),
                entry.kind.title_style(palette),
            )));
            for line in &entry.body {
                lines.push(Line::from(Span::styled(
                    if line.is_empty() {
                        " ".to_string()
                    } else {
                        format!("  {line}")
                    },
                    entry.kind.body_style(palette),
                )));
            }
            lines.push(Line::raw(""));
        }
        lines
    }

    fn copy_buffer_to_clipboard(&mut self) {
        let text = self.editor.buffer();
        if text.trim().is_empty() {
            self.status = "Buffer is empty. Nothing copied.".to_string();
            return;
        }
        match Clipboard::new().and_then(|mut clipboard| clipboard.set_text(text)) {
            Ok(()) => {
                self.status = format!(
                    "Copied {} line(s) to the system clipboard.",
                    self.editor.line_count()
                );
            }
            Err(err) => {
                self.status = format!("Clipboard copy failed: {err}");
            }
        }
    }

    fn paste_from_clipboard(&mut self) {
        match Clipboard::new().and_then(|mut clipboard| clipboard.get_text()) {
            Ok(text) => {
                if text.is_empty() {
                    self.status = "Clipboard is empty.".to_string();
                    return;
                }
                self.editor.insert_text(&text);
                self.status = "Pasted clipboard contents into the buffer.".to_string();
            }
            Err(err) => {
                self.status = format!("Clipboard paste failed: {err}");
            }
        }
    }

    fn apply_theme_command(&mut self, requested: Option<String>) {
        match requested {
            Some(name) => match normalize_ui_theme_name(&name) {
                Ok(theme_name) => {
                    self.theme_name = theme_name.clone();
                    self.status = format!("Theme switched to {theme_name}.");
                    self.push_output(
                        OutputKind::Info,
                        "Theme updated",
                        vec![format!("Active REPL theme: {}", self.theme_name)],
                    );
                }
                Err(err) => {
                    self.status = err.clone();
                    self.push_output(OutputKind::Error, "Theme update failed", vec![err]);
                }
            },
            None => {
                self.status = format!("Current theme: {}.", self.theme_name);
                self.push_output(
                    OutputKind::Info,
                    "Available themes",
                    vec![
                        format!("Current theme: {}", self.theme_name),
                        format!("Themes: {}", repl_theme_names().join(", ")),
                        "Use `.theme <name>` to switch the live REPL palette.".to_string(),
                    ],
                );
            }
        }
    }
}

#[derive(Debug, Clone)]
enum Submission {
    Empty,
    Help,
    Clear,
    Theme(Option<String>),
    Exit,
    Evaluate {
        source: String,
        stripped_run_directive: bool,
    },
}

fn collect_submission(buffer: &str) -> Submission {
    let normalized = normalize_script_source(buffer.to_string());
    if normalized.trim().is_empty() {
        return Submission::Empty;
    }

    let trimmed = normalized.trim();
    if let Some(directive) = ReplDirective::parse(trimmed) {
        return match directive {
            ReplDirective::Help => Submission::Help,
            ReplDirective::Clear => Submission::Clear,
            ReplDirective::Theme => Submission::Theme(
                parse_theme_argument(trimmed).expect("theme directive should parse"),
            ),
            ReplDirective::Exit => Submission::Exit,
            ReplDirective::Run => Submission::Empty,
        };
    }

    let lines = normalized.lines().collect::<Vec<_>>();
    if let Some(last_non_empty) = lines.iter().rposition(|line| !line.trim().is_empty()) {
        if matches!(
            ReplDirective::parse(lines[last_non_empty].trim()),
            Some(ReplDirective::Run)
        ) {
            let source = lines[..last_non_empty].join("\n");
            if source.trim().is_empty() {
                return Submission::Empty;
            }
            return Submission::Evaluate {
                source,
                stripped_run_directive: true,
            };
        }
    }

    Submission::Evaluate {
        source: normalized,
        stripped_run_directive: false,
    }
}

fn evaluation_lines(evaluation: &ReplEvaluation) -> Vec<String> {
    let mut lines = Vec::new();
    if let Some(value) = evaluation.visible_value.as_deref() {
        lines.extend(value.lines().map(|line| line.to_string()));
    }
    if lines.is_empty() && evaluation.execution_complete {
        lines.push("Execution complete".to_string());
    }
    lines
}

fn preview_source_headline(source: &str) -> String {
    let snippet = source
        .lines()
        .find(|line| !line.trim().is_empty())
        .unwrap_or("<empty>")
        .trim();
    const MAX_PREVIEW_CHARS: usize = 72;
    let preview = snippet.chars().take(MAX_PREVIEW_CHARS).collect::<String>();
    if snippet.chars().count() > MAX_PREVIEW_CHARS {
        format!("{preview}...")
    } else {
        preview
    }
}

fn centered_rect(percent_x: u16, percent_y: u16, area: Rect) -> Rect {
    let popup_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Percentage((100 - percent_y) / 2),
            Constraint::Percentage(percent_y),
            Constraint::Percentage((100 - percent_y) / 2),
        ])
        .split(area);

    Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage((100 - percent_x) / 2),
            Constraint::Percentage(percent_x),
            Constraint::Percentage((100 - percent_x) / 2),
        ])
        .split(popup_layout[1])[1]
}

#[derive(Debug, Clone)]
struct OutputEntry {
    kind: OutputKind,
    title: String,
    body: Vec<String>,
}

#[derive(Debug, Clone, Copy)]
enum OutputKind {
    Info,
    Success,
    Error,
}

impl OutputKind {
    fn title_style(self, palette: ReplPalette) -> Style {
        match self {
            Self::Info => Style::default()
                .fg(palette.title_info)
                .add_modifier(Modifier::BOLD),
            Self::Success => Style::default()
                .fg(palette.title_success)
                .add_modifier(Modifier::BOLD),
            Self::Error => Style::default()
                .fg(palette.title_error)
                .add_modifier(Modifier::BOLD),
        }
    }

    fn body_style(self, palette: ReplPalette) -> Style {
        match self {
            Self::Info => Style::default().fg(palette.text_muted),
            Self::Success => Style::default().fg(palette.text_primary),
            Self::Error => Style::default().fg(palette.text_primary),
        }
    }
}

#[derive(Debug, Clone)]
struct ReplEditor {
    lines: Vec<String>,
    cursor_row: usize,
    cursor_col: usize,
    scroll_x: usize,
    scroll_y: usize,
}

impl Default for ReplEditor {
    fn default() -> Self {
        Self {
            lines: vec![String::new()],
            cursor_row: 0,
            cursor_col: 0,
            scroll_x: 0,
            scroll_y: 0,
        }
    }
}

impl ReplEditor {
    fn buffer(&self) -> String {
        self.lines.join("\n")
    }

    fn set_text(&mut self, text: &str) {
        self.lines = if text.is_empty() {
            vec![String::new()]
        } else {
            text.lines().map(|line| line.to_string()).collect()
        };
        if self.lines.is_empty() {
            self.lines.push(String::new());
        }
        self.cursor_row = self.lines.len().saturating_sub(1);
        self.cursor_col = line_char_len(self.current_line());
        self.scroll_x = 0;
        self.scroll_y = 0;
    }

    fn clear(&mut self) {
        *self = Self::default();
    }

    fn line_count(&self) -> usize {
        self.lines.len()
    }

    fn char_count(&self) -> usize {
        self.lines
            .iter()
            .map(|line| line.chars().count())
            .sum::<usize>()
            + self.lines.len().saturating_sub(1)
    }

    fn gutter_width(&self) -> usize {
        self.line_count().max(1).to_string().len() + 2
    }

    fn render_line_numbers(&self, palette: ReplPalette) -> Text<'static> {
        let mut lines = Vec::with_capacity(self.lines.len());
        let width = self.gutter_width().saturating_sub(1);
        for (index, _) in self.lines.iter().enumerate() {
            let style = if index == self.cursor_row {
                Style::default()
                    .fg(palette.chrome_accent)
                    .bg(palette.panel_background_active)
                    .add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(palette.chrome_muted)
            };
            lines.push(Line::from(Span::styled(
                format!("{:>width$} ", index + 1, width = width),
                style,
            )));
        }
        Text::from(lines)
    }

    fn render_source_lines(&self, palette: ReplPalette) -> Text<'static> {
        let lines = self
            .lines
            .iter()
            .enumerate()
            .map(|(index, line)| highlight_source_line(line, index == self.cursor_row, palette))
            .collect::<Vec<_>>();
        Text::from(lines)
    }

    fn insert_char(&mut self, ch: char) {
        let cursor_col = self.cursor_col;
        let line = self.current_line_mut();
        let byte_index = char_to_byte_index(line, cursor_col);
        line.insert(byte_index, ch);
        self.cursor_col += 1;
    }

    fn insert_spaces(&mut self, count: usize) {
        for _ in 0..count {
            self.insert_char(' ');
        }
    }

    fn insert_text(&mut self, text: &str) {
        for ch in text.chars() {
            if ch == '\n' {
                self.insert_newline();
            } else if ch != '\r' {
                self.insert_char(ch);
            }
        }
    }

    fn insert_newline(&mut self) {
        let cursor_col = self.cursor_col;
        let current_row = self.cursor_row;
        let line = self.current_line_mut();
        let split_at = char_to_byte_index(line, cursor_col);
        let remainder = line.split_off(split_at);
        self.lines.insert(current_row + 1, remainder);
        self.cursor_row += 1;
        self.cursor_col = 0;
    }

    fn backspace(&mut self) {
        if self.cursor_col > 0 {
            let cursor_col = self.cursor_col;
            let line = self.current_line_mut();
            let start = char_to_byte_index(line, cursor_col - 1);
            let end = char_to_byte_index(line, cursor_col);
            line.replace_range(start..end, "");
            self.cursor_col -= 1;
            return;
        }

        if self.cursor_row == 0 {
            return;
        }

        let current = self.lines.remove(self.cursor_row);
        self.cursor_row -= 1;
        let previous_len = line_char_len(self.current_line());
        self.lines[self.cursor_row].push_str(&current);
        self.cursor_col = previous_len;
    }

    fn delete(&mut self) {
        let current_len = line_char_len(self.current_line());
        if self.cursor_col < current_len {
            let cursor_col = self.cursor_col;
            let line = self.current_line_mut();
            let start = char_to_byte_index(line, cursor_col);
            let end = char_to_byte_index(line, cursor_col + 1);
            line.replace_range(start..end, "");
            return;
        }

        if self.cursor_row + 1 >= self.lines.len() {
            return;
        }

        let next_line = self.lines.remove(self.cursor_row + 1);
        self.lines[self.cursor_row].push_str(&next_line);
    }

    fn move_left(&mut self) {
        if self.cursor_col > 0 {
            self.cursor_col -= 1;
        } else if self.cursor_row > 0 {
            self.cursor_row -= 1;
            self.cursor_col = line_char_len(self.current_line());
        }
    }

    fn move_right(&mut self) {
        let current_len = line_char_len(self.current_line());
        if self.cursor_col < current_len {
            self.cursor_col += 1;
        } else if self.cursor_row + 1 < self.lines.len() {
            self.cursor_row += 1;
            self.cursor_col = 0;
        }
    }

    fn move_up(&mut self) {
        if self.cursor_row == 0 {
            return;
        }
        self.cursor_row -= 1;
        self.cursor_col = self.cursor_col.min(line_char_len(self.current_line()));
    }

    fn move_down(&mut self) {
        if self.cursor_row + 1 >= self.lines.len() {
            return;
        }
        self.cursor_row += 1;
        self.cursor_col = self.cursor_col.min(line_char_len(self.current_line()));
    }

    fn move_home(&mut self) {
        self.cursor_col = 0;
    }

    fn move_end(&mut self) {
        self.cursor_col = line_char_len(self.current_line());
    }

    fn page_up(&mut self) {
        self.cursor_row = self.cursor_row.saturating_sub(10);
        self.cursor_col = self.cursor_col.min(line_char_len(self.current_line()));
    }

    fn page_down(&mut self) {
        self.cursor_row = (self.cursor_row + 10).min(self.lines.len().saturating_sub(1));
        self.cursor_col = self.cursor_col.min(line_char_len(self.current_line()));
    }

    fn ensure_visible(&mut self, viewport_width: usize, viewport_height: usize) {
        if viewport_height == 0 || viewport_width == 0 {
            return;
        }

        if self.cursor_row < self.scroll_y {
            self.scroll_y = self.cursor_row;
        } else if self.cursor_row >= self.scroll_y + viewport_height {
            self.scroll_y = self.cursor_row + 1 - viewport_height;
        }

        let prefix_width = cursor_prefix_width(self.current_line(), self.cursor_col);
        if prefix_width < self.scroll_x {
            self.scroll_x = prefix_width;
        } else if prefix_width >= self.scroll_x + viewport_width {
            self.scroll_x = prefix_width + 1 - viewport_width;
        }
    }

    fn cursor_screen_position(&self, area: Rect) -> (u16, u16) {
        let prefix_width = cursor_prefix_width(self.current_line(), self.cursor_col);
        let x = area
            .x
            .saturating_add(prefix_width.saturating_sub(self.scroll_x) as u16);
        let y = area
            .y
            .saturating_add(self.cursor_row.saturating_sub(self.scroll_y) as u16);
        (
            x.min(area.x + area.width.saturating_sub(1)),
            y.min(area.y + area.height.saturating_sub(1)),
        )
    }

    fn current_line(&self) -> &str {
        &self.lines[self.cursor_row]
    }

    fn current_line_mut(&mut self) -> &mut String {
        &mut self.lines[self.cursor_row]
    }
}

fn char_to_byte_index(line: &str, char_index: usize) -> usize {
    if char_index == 0 {
        return 0;
    }

    line.char_indices()
        .nth(char_index)
        .map(|(index, _)| index)
        .unwrap_or_else(|| line.len())
}

fn line_char_len(line: &str) -> usize {
    line.chars().count()
}

fn cursor_prefix_width(line: &str, cursor_col: usize) -> usize {
    let byte_index = char_to_byte_index(line, cursor_col);
    UnicodeWidthStr::width(&line[..byte_index])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn editor_backspace_merges_lines() {
        let mut editor = ReplEditor::default();
        editor.insert_text("fn main()");
        editor.insert_newline();
        editor.insert_text("    return 7");
        editor.move_home();
        editor.backspace();

        assert_eq!(editor.line_count(), 1);
        assert_eq!(editor.buffer(), "fn main()    return 7");
    }

    #[test]
    fn trailing_run_directive_is_stripped_before_eval() {
        match collect_submission("fn main() -> Int:\n    return 7\n.run") {
            Submission::Evaluate {
                source,
                stripped_run_directive,
            } => {
                assert!(stripped_run_directive);
                assert_eq!(source, "fn main() -> Int:\n    return 7");
            }
            other => panic!("expected evaluation submission, got {other:?}"),
        }
    }

    #[test]
    fn theme_directive_is_routed_to_theme_submission() {
        match collect_submission(".theme plain") {
            Submission::Theme(Some(theme)) => assert_eq!(theme, "plain"),
            other => panic!("expected theme submission, got {other:?}"),
        }
    }
}
