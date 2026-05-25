use arboard::Clipboard;
use kain_core::tooling_config::normalize_ui_theme_name;
use std::fs;
use std::io;
use std::path::PathBuf;
use std::time::{Duration, Instant};

use crossterm::event::{
    self, DisableBracketedPaste, DisableMouseCapture, EnableBracketedPaste, EnableMouseCapture,
    Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers, MouseButton, MouseEvent, MouseEventKind,
};
use crossterm::execute;
use crossterm::terminal::{
    disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use ratatui::backend::CrosstermBackend;
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::style::{Modifier, Style};
use ratatui::text::{Line, Span, Text};
use ratatui::widgets::{Block, Borders, Clear, Paragraph, Wrap};
use ratatui::{Frame, Terminal};
use unicode_width::UnicodeWidthStr;

use crate::command::{parse_open_argument, parse_theme_argument, ReplDirective};
use crate::evaluation::{ReplEvaluation, ReplEvaluator};
use crate::highlight::highlight_source_line;
use crate::source::normalize_script_source;
use crate::terminal::ReplTerminalConfig;
use crate::theme::{
    active_repl_theme_name, cycle_repl_theme_name, repl_palette, repl_theme_names, ReplPalette,
};

const TAB_WIDTH: usize = 4;
const OUTPUT_HISTORY_LIMIT: usize = 64;
const MIN_GUTTER_DIGITS: usize = 4;
const UNDO_HISTORY_LIMIT: usize = 256;

pub fn run_tui_repl(config: ReplTerminalConfig) -> io::Result<()> {
    let mut stdout = io::stdout();
    enable_raw_mode()?;
    execute!(
        stdout,
        EnterAlternateScreen,
        EnableBracketedPaste,
        EnableMouseCapture
    )?;

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
    execute!(
        terminal.backend_mut(),
        DisableBracketedPaste,
        DisableMouseCapture,
        LeaveAlternateScreen
    )?;
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
            Event::Paste(content) => app.handle_paste(content),
            Event::Mouse(mouse) => app.handle_mouse(mouse),
            Event::Resize(_, _) => {}
            _ => {}
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum PromptKind {
    SaveFile,
    OpenFile,
}

#[derive(Debug, Clone)]
struct PromptState {
    kind: PromptKind,
    input: String,
}

#[derive(Debug, Clone, Copy)]
struct ClickState {
    point: CursorPoint,
    at: Instant,
    count: u8,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct CursorPoint {
    row: usize,
    col: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct SelectionRange {
    start: CursorPoint,
    end: CursorPoint,
}

impl SelectionRange {
    fn normalize(self) -> Self {
        if cursor_point_leq(self.start, self.end) {
            self
        } else {
            Self {
                start: self.end,
                end: self.start,
            }
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum RunSlice {
    Selection(String),
    Block(String),
}

#[derive(Debug, Clone)]
struct ReplApp {
    config: ReplTerminalConfig,
    evaluator: ReplEvaluator,
    editor: ReplEditor,
    theme_name: String,
    output_log: Vec<OutputEntry>,
    show_help: bool,
    prompt: Option<PromptState>,
    last_save_path: String,
    last_open_path: String,
    current_file_path: Option<String>,
    recent_files: Vec<String>,
    clean_buffer_snapshot: String,
    should_quit: bool,
    next_run_id: usize,
    status: String,
    cwd_display: String,
    editor_text_area: Rect,
    output_text_area: Rect,
    click_state: Option<ClickState>,
    output_selection_anchor: Option<CursorPoint>,
    output_selection_cursor: Option<CursorPoint>,
}

impl ReplApp {
    fn new(config: ReplTerminalConfig) -> Self {
        Self {
            config,
            evaluator: ReplEvaluator::default(),
            editor: ReplEditor::default(),
            theme_name: active_repl_theme_name(),
            output_log: Vec::new(),
            show_help: false,
            prompt: None,
            last_save_path: "file.kn".to_string(),
            last_open_path: ".".to_string(),
            current_file_path: None,
            recent_files: Vec::new(),
            clean_buffer_snapshot: String::new(),
            should_quit: false,
            next_run_id: 0,
            status: "Idle".to_string(),
            cwd_display: std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .display()
                .to_string(),
            editor_text_area: Rect::default(),
            output_text_area: Rect::default(),
            click_state: None,
            output_selection_anchor: None,
            output_selection_cursor: None,
        }
    }

    fn handle_key(&mut self, key: KeyEvent) {
        if self.prompt.is_some() {
            self.handle_prompt_key(key);
            return;
        }

        let selecting = key.modifiers.contains(KeyModifiers::SHIFT);

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
                code: KeyCode::Char('s'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.open_save_prompt(),
            KeyEvent {
                code: KeyCode::Char('o'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.open_open_prompt(),
            KeyEvent {
                code: KeyCode::Char('z'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL)
                && modifiers.contains(KeyModifiers::SHIFT) =>
            {
                self.redo_editor_change()
            }
            KeyEvent {
                code: KeyCode::Char('z'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.undo_editor_change(),
            KeyEvent {
                code: KeyCode::Char('y'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => self.redo_editor_change(),
            KeyEvent {
                code: KeyCode::Enter,
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL)
                && modifiers.contains(KeyModifiers::SHIFT) =>
            {
                self.run_selection_or_current_block();
            }
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
                code: KeyCode::F(6),
                ..
            } => self.run_selection_or_current_block(),
            KeyEvent {
                code: KeyCode::F(7),
                ..
            } => self.run_current_line(),
            KeyEvent {
                code: KeyCode::F(8),
                ..
            } => self.run_function_under_cursor(),
            KeyEvent {
                code: KeyCode::F(1),
                ..
            } => self.show_help = !self.show_help,
            KeyEvent {
                code: KeyCode::F(2),
                modifiers,
                ..
            } => self.cycle_theme(modifiers.contains(KeyModifiers::SHIFT)),
            KeyEvent {
                code: KeyCode::Char('l'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.editor.clear();
                self.status = "Buffer cleared".to_string();
            }
            KeyEvent {
                code: KeyCode::Char('k'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                self.output_log.clear();
                self.clear_output_selection();
                self.status = "Output cleared".to_string();
            }
            KeyEvent {
                code: KeyCode::Esc, ..
            } => {
                self.show_help = false;
                self.editor.clear_selection();
            }
            KeyEvent {
                code: KeyCode::Left,
                ..
            } => self.editor.move_left(selecting),
            KeyEvent {
                code: KeyCode::Right,
                ..
            } => self.editor.move_right(selecting),
            KeyEvent {
                code: KeyCode::Up, ..
            } => self.editor.move_up(selecting),
            KeyEvent {
                code: KeyCode::Down,
                ..
            } => self.editor.move_down(selecting),
            KeyEvent {
                code: KeyCode::Home,
                ..
            } => self.editor.move_home(selecting),
            KeyEvent {
                code: KeyCode::End, ..
            } => self.editor.move_end(selecting),
            KeyEvent {
                code: KeyCode::PageUp,
                ..
            } => self.editor.page_up(selecting),
            KeyEvent {
                code: KeyCode::PageDown,
                ..
            } => self.editor.page_down(selecting),
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

    fn handle_paste(&mut self, content: String) {
        if let Some(prompt) = &mut self.prompt {
            prompt.input.push_str(&content.replace(['\r', '\n'], ""));
            return;
        }
        self.editor.insert_text(&content);
        self.status = "Pasted".to_string();
    }

    fn handle_mouse(&mut self, mouse: MouseEvent) {
        if self.show_help || self.prompt.is_some() {
            return;
        }

        if self.point_in_rect(mouse.column, mouse.row, self.output_text_area) {
            self.handle_output_mouse(mouse);
            return;
        }

        match mouse.kind {
            MouseEventKind::Down(MouseButton::Left) => {
                if let Some(point) = self.editor_point_from_screen(mouse.column, mouse.row) {
                    self.clear_output_selection();
                    match self.register_click(point) {
                        1 => self.editor.start_mouse_selection(point),
                        2 => self.editor.select_word_at(point),
                        _ => self.editor.select_line_at(point),
                    }
                }
            }
            MouseEventKind::Drag(MouseButton::Left) => {
                if let Some(point) = self.editor_point_from_drag(mouse.column, mouse.row) {
                    self.editor.drag_mouse_selection(point);
                }
            }
            MouseEventKind::Up(MouseButton::Left) => self.editor.finish_mouse_selection(),
            MouseEventKind::ScrollUp => self.editor.scroll_lines(-3),
            MouseEventKind::ScrollDown => self.editor.scroll_lines(3),
            _ => {}
        }
    }

    fn handle_output_mouse(&mut self, mouse: MouseEvent) {
        match mouse.kind {
            MouseEventKind::Down(MouseButton::Left) => {
                if let Some(point) = self.output_point_from_screen(mouse.column, mouse.row) {
                    self.editor.clear_selection();
                    self.output_selection_anchor = Some(point);
                    self.output_selection_cursor = Some(point);
                }
            }
            MouseEventKind::Drag(MouseButton::Left) => {
                if let Some(point) = self.output_point_from_screen(mouse.column, mouse.row) {
                    if self.output_selection_anchor.is_none() {
                        self.output_selection_anchor = Some(point);
                    }
                    self.output_selection_cursor = Some(point);
                }
            }
            MouseEventKind::Up(MouseButton::Left) => {
                if self.output_selection_anchor == self.output_selection_cursor {
                    self.clear_output_selection();
                }
            }
            _ => {}
        }
    }

    fn register_click(&mut self, point: CursorPoint) -> u8 {
        let now = Instant::now();
        let next_count = match self.click_state {
            Some(state)
                if state.point == point
                    && now.duration_since(state.at) <= Duration::from_millis(450) =>
            {
                if state.count >= 3 {
                    1
                } else {
                    state.count + 1
                }
            }
            _ => 1,
        };
        self.click_state = Some(ClickState {
            point,
            at: now,
            count: next_count,
        });
        next_count
    }

    fn run_current_buffer(&mut self) {
        match collect_submission(&self.editor.buffer()) {
            Submission::Empty => {
                self.status = "Empty buffer".to_string();
            }
            Submission::Help => {
                self.show_help = true;
                self.status = "Commands".to_string();
            }
            Submission::Clear => {
                self.editor.clear();
                self.status = "Buffer cleared".to_string();
            }
            Submission::Theme(theme) => self.apply_theme_command(theme),
            Submission::Open(path) => match path {
                Some(path) => self.open_from_path(&path),
                None => self.open_open_prompt(),
            },
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

    fn run_selection_or_current_block(&mut self) {
        match self.editor.selection_or_current_block() {
            Some(RunSlice::Selection(source)) => self.evaluate_named_source("Selection", source),
            Some(RunSlice::Block(source)) => self.evaluate_named_source("Block", source),
            None => {
                self.status = "Empty block".to_string();
            }
        }
    }

    fn run_current_line(&mut self) {
        match self.editor.current_line_text() {
            Some(source) => self.evaluate_named_source("Line", source),
            None => self.status = "Empty line".to_string(),
        }
    }

    fn run_function_under_cursor(&mut self) {
        match self.editor.current_function_text() {
            Some(source) => self.evaluate_named_source("Function", source),
            None => self.status = "No function".to_string(),
        }
    }

    fn evaluate_source(&mut self, source: String) {
        self.evaluate_named_source("Run", source);
    }

    fn evaluate_named_source(&mut self, run_label: &str, source: String) {
        self.next_run_id += 1;
        let run_id = self.next_run_id;
        let line_count = source.lines().count().max(1);
        let headline = format!(
            "{run_label} #{run_id} · {line_count} lines · {}",
            preview_source_headline(&source)
        );

        match self
            .evaluator
            .evaluate_source(&self.config.source_name, &source)
        {
            Ok(evaluation) => {
                self.push_output(OutputKind::Success, headline, evaluation_lines(&evaluation));
                self.status = format!("{run_label} #{run_id} clean");
            }
            Err(error) => {
                let body = error
                    .plain_text()
                    .lines()
                    .map(|line| line.to_string())
                    .collect::<Vec<_>>();
                self.push_output(OutputKind::Error, headline, body);
                self.status = format!("{run_label} #{run_id} diagnostics");
            }
        }
    }

    fn push_output(&mut self, kind: OutputKind, title: impl Into<String>, body: Vec<String>) {
        self.output_log.push(OutputEntry {
            kind,
            title: title.into(),
            body,
        });
        self.clear_output_selection();
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
                Constraint::Length(5),
                Constraint::Min(10),
                Constraint::Length(1),
            ])
            .split(area);

        self.render_header(frame, layout[0], palette);
        self.render_body(frame, layout[1], palette);
        self.render_status(frame, layout[2], palette);

        if self.show_help {
            self.render_help_overlay(frame, area, palette);
        } else if self.prompt.is_some() {
            self.render_prompt_overlay(frame, area, palette);
        }
    }

    fn render_header(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let file_label = self
            .current_file_path
            .as_deref()
            .unwrap_or(self.config.source_name.as_str());
        let dirty_label = if self.is_dirty() {
            " modified "
        } else {
            " clean "
        };
        let dirty_style = if self.is_dirty() {
            Style::default()
                .fg(palette.keyword_effect)
                .add_modifier(Modifier::BOLD)
        } else {
            Style::default().fg(palette.keyword_world)
        };
        let file_chips = self.render_file_chips(palette);
        let banner = Paragraph::new(Text::from(vec![
            Line::from(vec![
                Span::styled(" Kain REPL ", palette.title_style()),
                Span::styled(
                    format!("[{}] ", self.theme_name),
                    Style::default().fg(palette.chrome_secondary),
                ),
                Span::styled(
                    self.config.metadata.banner(),
                    Style::default().fg(palette.text_muted),
                ),
            ]),
            Line::from(vec![
                Span::styled(" cwd ", Style::default().fg(palette.chrome_secondary)),
                Span::styled(
                    self.cwd_display.clone(),
                    Style::default().fg(palette.text_muted),
                ),
                Span::styled("  file ", Style::default().fg(palette.chrome_secondary)),
                Span::styled(
                    file_label.to_string(),
                    Style::default().fg(palette.text_muted),
                ),
                Span::styled("  state ", Style::default().fg(palette.chrome_secondary)),
                Span::styled(dirty_label, dirty_style),
            ]),
            file_chips,
        ]))
        .block(
            Block::default()
                .borders(Borders::ALL)
                .border_style(Style::default().fg(palette.border_focus))
                .style(Style::default().bg(palette.panel_background)),
        );
        frame.render_widget(banner, area);
    }

    fn render_file_chips(&self, palette: ReplPalette) -> Line<'static> {
        let mut spans = vec![Span::styled(
            " files ",
            Style::default().fg(palette.chrome_secondary),
        )];
        let current = self
            .current_file_path
            .clone()
            .unwrap_or_else(|| self.config.source_name.clone());
        spans.push(self.file_chip_span(&current, true, palette));

        for path in self
            .recent_files
            .iter()
            .filter(|path| **path != current)
            .take(4)
        {
            spans.push(Span::raw(" "));
            spans.push(self.file_chip_span(path, false, palette));
        }

        Line::from(spans)
    }

    fn file_chip_span(&self, path: &str, active: bool, palette: ReplPalette) -> Span<'static> {
        let label = short_file_chip_label(path);
        let style = if active {
            let text = if self.is_dirty() && self.current_file_path.as_deref() == Some(path) {
                format!("[{label}*]")
            } else {
                format!("[{label}]")
            };
            return Span::styled(
                text,
                Style::default()
                    .fg(palette.panel_background)
                    .bg(palette.border_focus)
                    .add_modifier(Modifier::BOLD),
            );
        } else {
            Style::default()
                .fg(palette.text_muted)
                .bg(palette.panel_background_active)
        };
        Span::styled(format!("[{label}]"), style)
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
        self.editor_text_area = columns[1];

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

        if self.prompt.is_none() && !self.show_help {
            let (cursor_x, cursor_y) = self.editor.cursor_screen_position(columns[1]);
            frame.set_cursor_position((cursor_x, cursor_y));
        }
    }

    fn render_output(&mut self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let block = Block::default()
            .title(format!(" Output · {} entries ", self.output_log.len()))
            .borders(Borders::ALL)
            .border_style(Style::default().fg(palette.border))
            .style(Style::default().bg(palette.panel_background));
        let inner = block.inner(area);
        self.output_text_area = inner;
        frame.render_widget(block, area);

        let plain_lines = self.output_plain_lines();
        let lines = self.output_lines(palette, &plain_lines);
        let scroll_y = output_scroll_y(plain_lines.len(), inner.height as usize)
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
            " Ln {}, Col {} | {} | Ctrl+Enter run | Ctrl+Shift+Enter block | F7 line | F8 fn | F2 theme | Ctrl+O open | Ctrl+S save | Ctrl+Q quit ",
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
        let popup = centered_rect(52, 48, area);
        let help_text = Text::from(vec![
            Line::from(Span::styled("Keys", palette.title_style())),
            Line::raw(""),
            Line::raw("Ctrl+Enter / F5  run"),
            Line::raw("Ctrl+Shift+Enter run selection or block"),
            Line::raw("F6                run selection or block"),
            Line::raw("F7                run current line"),
            Line::raw("F8                run function under cursor"),
            Line::raw("F2                next theme"),
            Line::raw("Shift+F2          previous theme"),
            Line::raw("Ctrl+Shift+C      copy"),
            Line::raw("Ctrl+Shift+V      paste"),
            Line::raw("Ctrl+S            save file"),
            Line::raw("Ctrl+O            open file"),
            Line::raw("Ctrl+Z            undo"),
            Line::raw("Ctrl+Y            redo"),
            Line::raw("Ctrl+Shift+Z      redo"),
            Line::raw("Ctrl+L            clear buffer"),
            Line::raw("Ctrl+K            clear output"),
            Line::raw("Ctrl+Q            quit"),
            Line::raw("Esc / F1          close"),
            Line::raw(""),
            Line::from(Span::styled("Directives", palette.title_style())),
            Line::raw(".run"),
            Line::raw(".clear"),
            Line::raw(".theme"),
            Line::raw(".theme <name>"),
            Line::raw(".open <path>"),
            Line::raw(".quit"),
            Line::raw(""),
            Line::raw(format!("Themes: {}", repl_theme_names().join(", "))),
        ]);

        frame.render_widget(Clear, popup);
        frame.render_widget(
            Paragraph::new(help_text)
                .block(
                    Block::default()
                        .title(" Commands ")
                        .borders(Borders::ALL)
                        .border_style(Style::default().fg(palette.border_focus))
                        .style(Style::default().bg(palette.panel_background)),
                )
                .wrap(Wrap { trim: false }),
            popup,
        );
    }

    fn render_prompt_overlay(&self, frame: &mut Frame<'_>, area: Rect, palette: ReplPalette) {
        let popup = centered_rect(48, 22, area);
        let Some(prompt) = &self.prompt else {
            return;
        };
        let (title, hint) = match prompt.kind {
            PromptKind::SaveFile => ("Save File", "Enter save file  Esc cancel"),
            PromptKind::OpenFile => ("Open File", "Enter open file  Esc cancel"),
        };
        let input = prompt.input.as_str();
        let content = Text::from(vec![
            Line::from(Span::styled(title, palette.title_style())),
            Line::raw(""),
            Line::raw("Path"),
            Line::from(Span::styled(
                format!(" {input} "),
                Style::default()
                    .fg(palette.text_primary)
                    .bg(palette.panel_background_active),
            )),
            Line::raw(""),
            Line::from(Span::styled(hint, Style::default().fg(palette.text_muted))),
        ]);

        frame.render_widget(Clear, popup);
        frame.render_widget(
            Paragraph::new(content).block(
                Block::default()
                    .title(match prompt.kind {
                        PromptKind::SaveFile => " Save ",
                        PromptKind::OpenFile => " Open ",
                    })
                    .borders(Borders::ALL)
                    .border_style(Style::default().fg(palette.border_focus))
                    .style(Style::default().bg(palette.panel_background)),
            ),
            popup,
        );

        let cursor_x = popup
            .x
            .saturating_add(2)
            .saturating_add(input.chars().count() as u16);
        let cursor_y = popup.y.saturating_add(4);
        frame.set_cursor_position((cursor_x, cursor_y));
    }

    fn output_plain_lines(&self) -> Vec<String> {
        if self.output_log.is_empty() {
            return vec!["No output.".to_string()];
        }

        let mut lines = Vec::new();
        for entry in &self.output_log {
            lines.push(entry.title.clone());
            for line in &entry.body {
                lines.push(if line.is_empty() {
                    " ".to_string()
                } else {
                    format!("  {line}")
                });
            }
            lines.push(String::new());
        }
        lines
    }

    fn output_lines(&self, palette: ReplPalette, plain_lines: &[String]) -> Vec<Line<'static>> {
        let selection = self.output_selection_range();
        plain_lines
            .iter()
            .enumerate()
            .map(|(index, text)| {
                let style = if self.output_log.is_empty() {
                    palette.muted_style()
                } else {
                    output_line_style(index, &self.output_log, palette)
                };
                styled_text_line(
                    text,
                    style,
                    selection.and_then(|range| {
                        output_selection_bounds_for_line(range, index, text.chars().count())
                    }),
                )
            })
            .collect()
    }

    fn copy_buffer_to_clipboard(&mut self) {
        let text = self
            .selected_output_text()
            .or_else(|| self.editor.selected_text())
            .unwrap_or_else(|| self.editor.buffer());
        if text.trim().is_empty() {
            self.status = "Copy skipped".to_string();
            return;
        }
        match Clipboard::new().and_then(|mut clipboard| clipboard.set_text(text)) {
            Ok(()) => {
                self.status = if self.output_selection_range().is_some() {
                    "Copied output".to_string()
                } else if self.editor.has_selection() {
                    "Copied selection".to_string()
                } else {
                    format!("Copied {} line(s)", self.editor.line_count())
                };
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
                    self.status = "Clipboard empty".to_string();
                    return;
                }
                self.editor.insert_text(&text);
                self.status = "Pasted".to_string();
            }
            Err(err) => {
                self.status = format!("Clipboard paste failed: {err}");
            }
        }
    }

    fn open_save_prompt(&mut self) {
        self.show_help = false;
        let default_path = self
            .current_file_path
            .clone()
            .unwrap_or_else(|| self.last_save_path.clone());
        self.prompt = Some(PromptState {
            kind: PromptKind::SaveFile,
            input: default_path,
        });
        self.status = "Save file".to_string();
    }

    fn open_open_prompt(&mut self) {
        self.show_help = false;
        let default_path = self
            .current_file_path
            .clone()
            .unwrap_or_else(|| self.last_open_path.clone());
        self.prompt = Some(PromptState {
            kind: PromptKind::OpenFile,
            input: default_path,
        });
        self.status = "Open file".to_string();
    }

    fn handle_prompt_key(&mut self, key: KeyEvent) {
        match key {
            KeyEvent {
                code: KeyCode::Esc, ..
            } => {
                self.prompt = None;
                self.status = "Prompt cancelled".to_string();
            }
            KeyEvent {
                code: KeyCode::Enter,
                ..
            } => self.commit_prompt(),
            KeyEvent {
                code: KeyCode::Char('v'),
                modifiers,
                ..
            } if modifiers.contains(KeyModifiers::CONTROL) => {
                if let Ok(text) = Clipboard::new().and_then(|mut clipboard| clipboard.get_text()) {
                    if let Some(prompt) = &mut self.prompt {
                        prompt.input.push_str(&text.replace(['\r', '\n'], ""));
                    }
                }
            }
            KeyEvent {
                code: KeyCode::Backspace,
                ..
            } => {
                if let Some(prompt) = &mut self.prompt {
                    prompt.input.pop();
                }
            }
            KeyEvent {
                code: KeyCode::Char(ch),
                modifiers,
                ..
            } if !modifiers.contains(KeyModifiers::CONTROL)
                && !modifiers.contains(KeyModifiers::ALT) =>
            {
                if let Some(prompt) = &mut self.prompt {
                    prompt.input.push(ch);
                }
            }
            _ => {}
        }
    }

    fn commit_prompt(&mut self) {
        let Some(prompt) = self.prompt.take() else {
            return;
        };
        let trimmed = prompt.input.trim();
        if trimmed.is_empty() {
            self.status = "Path required".to_string();
            return;
        }

        match prompt.kind {
            PromptKind::SaveFile => self.save_to_path(trimmed),
            PromptKind::OpenFile => self.open_from_path(trimmed),
        }
    }

    fn save_to_path(&mut self, raw_path: &str) {
        let absolute = resolve_prompt_path(raw_path);
        let source = self.editor.buffer();
        match fs::write(&absolute, source) {
            Ok(()) => {
                let display = absolute.display().to_string();
                self.track_open_file(&display);
                self.clean_buffer_snapshot = self.editor.buffer();
                self.status = format!("Saved {display}");
                self.push_output(OutputKind::Info, "Saved", vec![display]);
            }
            Err(err) => {
                self.status = format!("Save failed: {err}");
                self.push_output(OutputKind::Error, "Save failed", vec![err.to_string()]);
            }
        }
    }

    fn open_from_path(&mut self, raw_path: &str) {
        let absolute = resolve_prompt_path(raw_path);
        match fs::read_to_string(&absolute) {
            Ok(source) => {
                let display = absolute.display().to_string();
                self.editor.load_text_at_start(&source);
                self.editor.clear_selection();
                self.clear_output_selection();
                self.track_open_file(&display);
                self.clean_buffer_snapshot = self.editor.buffer();
                self.status = format!("Opened {display}");
                self.push_output(OutputKind::Info, "Opened", vec![display]);
            }
            Err(err) => {
                self.status = format!("Open failed: {err}");
                self.push_output(OutputKind::Error, "Open failed", vec![err.to_string()]);
            }
        }
    }

    fn editor_point_from_screen(&self, column: u16, row: u16) -> Option<CursorPoint> {
        if column < self.editor_text_area.x
            || row < self.editor_text_area.y
            || column >= self.editor_text_area.x + self.editor_text_area.width
            || row >= self.editor_text_area.y + self.editor_text_area.height
        {
            return None;
        }

        let visual_row = (row - self.editor_text_area.y) as usize;
        let visual_col = (column - self.editor_text_area.x) as usize;
        let target_row = self.editor.scroll_y + visual_row;
        let line = self.editor.line_for_row(target_row);
        let target_col = visual_width_to_cursor_col(line, self.editor.scroll_x + visual_col);
        Some(CursorPoint {
            row: target_row,
            col: target_col,
        })
    }

    fn editor_point_from_drag(&mut self, column: u16, row: u16) -> Option<CursorPoint> {
        if self.editor_text_area.width == 0 || self.editor_text_area.height == 0 {
            return None;
        }

        let top = self.editor_text_area.y;
        let bottom = self.editor_text_area.y + self.editor_text_area.height.saturating_sub(1);
        let left = self.editor_text_area.x;
        let right = self.editor_text_area.x + self.editor_text_area.width.saturating_sub(1);

        if row < top {
            self.editor.scroll_lines(-2);
        } else if row > bottom {
            self.editor.scroll_lines(2);
        }

        let clamped_row = row.clamp(top, bottom);
        let clamped_col = column.clamp(left, right);
        self.editor_point_from_screen(clamped_col, clamped_row)
    }

    fn output_point_from_screen(&self, column: u16, row: u16) -> Option<CursorPoint> {
        if !self.point_in_rect(column, row, self.output_text_area) {
            return None;
        }
        let plain_lines = self.output_plain_lines();
        let scroll_y = output_scroll_y(plain_lines.len(), self.output_text_area.height as usize);
        let line_index = scroll_y + (row - self.output_text_area.y) as usize;
        let line = plain_lines
            .get(line_index)
            .map(String::as_str)
            .unwrap_or("");
        let col = visual_width_to_cursor_col(line, (column - self.output_text_area.x) as usize);
        Some(CursorPoint {
            row: line_index,
            col,
        })
    }

    fn point_in_rect(&self, column: u16, row: u16, rect: Rect) -> bool {
        rect.width > 0
            && rect.height > 0
            && column >= rect.x
            && row >= rect.y
            && column < rect.x + rect.width
            && row < rect.y + rect.height
    }

    fn clear_output_selection(&mut self) {
        self.output_selection_anchor = None;
        self.output_selection_cursor = None;
    }

    fn selected_output_text(&self) -> Option<String> {
        let selection = self.output_selection_range()?;
        let plain_lines = self.output_plain_lines();
        let mut chunks = Vec::new();
        for row in selection.start.row..=selection.end.row {
            let line = plain_lines.get(row).map(String::as_str).unwrap_or("");
            let start_col = if row == selection.start.row {
                selection.start.col.min(line_char_len(line))
            } else {
                0
            };
            let end_col = if row == selection.end.row {
                selection.end.col.min(line_char_len(line))
            } else {
                line_char_len(line)
            };
            let start_byte = char_to_byte_index(line, start_col);
            let end_byte = char_to_byte_index(line, end_col);
            chunks.push(line[start_byte..end_byte].to_string());
        }
        Some(chunks.join("\n"))
    }

    fn output_selection_range(&self) -> Option<SelectionRange> {
        let anchor = self.output_selection_anchor?;
        let cursor = self.output_selection_cursor?;
        if anchor == cursor {
            None
        } else {
            Some(
                SelectionRange {
                    start: anchor,
                    end: cursor,
                }
                .normalize(),
            )
        }
    }

    fn track_open_file(&mut self, path: &str) {
        self.last_open_path = path.to_string();
        self.last_save_path = path.to_string();
        self.current_file_path = Some(path.to_string());
        self.config.source_name = path.to_string();
        self.recent_files.retain(|existing| existing != path);
        self.recent_files.insert(0, path.to_string());
        self.recent_files.truncate(6);
    }

    fn is_dirty(&self) -> bool {
        self.editor.buffer() != self.clean_buffer_snapshot
    }

    fn cycle_theme(&mut self, reverse: bool) {
        let next_theme = cycle_repl_theme_name(&self.theme_name, reverse);
        self.theme_name = next_theme.clone();
        self.status = "Theme changed".to_string();
    }

    fn undo_editor_change(&mut self) {
        if self.editor.undo() {
            self.status = "Undo".to_string();
        } else {
            self.status = "Undo empty".to_string();
        }
    }

    fn redo_editor_change(&mut self) {
        if self.editor.redo() {
            self.status = "Redo".to_string();
        } else {
            self.status = "Redo empty".to_string();
        }
    }

    fn apply_theme_command(&mut self, requested: Option<String>) {
        match requested {
            Some(name) => match normalize_ui_theme_name(&name) {
                Ok(theme_name) => {
                    self.theme_name = theme_name.clone();
                    self.status = "Theme changed".to_string();
                }
                Err(err) => {
                    self.status = err.clone();
                    self.push_output(OutputKind::Error, "Theme error", vec![err]);
                }
            },
            None => {
                self.status = "Theme".to_string();
                self.push_output(
                    OutputKind::Info,
                    "Theme",
                    vec![
                        format!("Current: {}", self.theme_name),
                        format!("Available: {}", repl_theme_names().join(", ")),
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
    Open(Option<String>),
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
            ReplDirective::Open => {
                Submission::Open(parse_open_argument(trimmed).expect("open directive should parse"))
            }
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

fn resolve_prompt_path(raw_path: &str) -> PathBuf {
    let path = PathBuf::from(raw_path);
    if path.is_absolute() {
        path
    } else {
        std::env::current_dir()
            .unwrap_or_else(|_| PathBuf::from("."))
            .join(path)
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

fn cursor_point_leq(left: CursorPoint, right: CursorPoint) -> bool {
    left.row < right.row || (left.row == right.row && left.col <= right.col)
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
    desired_col: usize,
    scroll_x: usize,
    scroll_y: usize,
    follow_cursor: bool,
    selection_anchor: Option<CursorPoint>,
    mouse_selecting: bool,
    undo_stack: Vec<EditorSnapshot>,
    redo_stack: Vec<EditorSnapshot>,
}

impl Default for ReplEditor {
    fn default() -> Self {
        Self {
            lines: vec![String::new()],
            cursor_row: 0,
            cursor_col: 0,
            desired_col: 0,
            scroll_x: 0,
            scroll_y: 0,
            follow_cursor: true,
            selection_anchor: None,
            mouse_selecting: false,
            undo_stack: Vec::new(),
            redo_stack: Vec::new(),
        }
    }
}

impl ReplEditor {
    fn buffer(&self) -> String {
        self.lines.join("\n")
    }

    fn has_selection(&self) -> bool {
        self.selection_range().is_some()
    }

    fn clear_selection(&mut self) {
        self.selection_anchor = None;
        self.mouse_selecting = false;
    }

    fn line_for_row(&self, row: usize) -> &str {
        self.lines.get(row).map(String::as_str).unwrap_or("")
    }

    fn selected_text(&self) -> Option<String> {
        let selection = self.selection_range()?;
        let mut chunks = Vec::new();
        for row in selection.start.row..=selection.end.row {
            let line = self.line_for_row(row);
            let start_col = if row == selection.start.row {
                selection.start.col.min(line_char_len(line))
            } else {
                0
            };
            let end_col = if row == selection.end.row {
                selection.end.col.min(line_char_len(line))
            } else {
                line_char_len(line)
            };
            let start_byte = char_to_byte_index(line, start_col);
            let end_byte = char_to_byte_index(line, end_col);
            chunks.push(line[start_byte..end_byte].to_string());
        }
        Some(chunks.join("\n"))
    }

    fn set_text(&mut self, text: &str) {
        self.push_undo_state();
        self.set_text_raw(text);
    }

    fn load_text_at_start(&mut self, text: &str) {
        self.lines = if text.is_empty() {
            vec![String::new()]
        } else {
            text.lines().map(|line| line.to_string()).collect()
        };
        if self.lines.is_empty() {
            self.lines.push(String::new());
        }
        self.cursor_row = 0;
        self.cursor_col = 0;
        self.desired_col = 0;
        self.scroll_x = 0;
        self.scroll_y = 0;
        self.follow_cursor = true;
        self.clear_selection();
    }

    fn set_text_raw(&mut self, text: &str) {
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
        self.desired_col = self.cursor_col;
        self.scroll_x = 0;
        self.scroll_y = 0;
        self.follow_cursor = true;
        self.clear_selection();
    }

    fn clear(&mut self) {
        self.push_undo_state();
        self.clear_raw();
    }

    fn clear_raw(&mut self) {
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
        self.line_count()
            .max(1)
            .to_string()
            .len()
            .max(MIN_GUTTER_DIGITS)
            + 2
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
            .map(|(index, line)| {
                let mut display_line = line.to_string();
                let line_is_selected = self
                    .selection_range()
                    .map(|selection| index >= selection.start.row && index <= selection.end.row)
                    .unwrap_or(false);
                if index == self.cursor_row {
                    let virtual_padding = self.cursor_col.saturating_sub(line_char_len(line));
                    if virtual_padding > 0 {
                        display_line.push_str(&" ".repeat(virtual_padding));
                    } else if display_line.is_empty() {
                        display_line.push(' ');
                    }
                } else if display_line.is_empty() && line_is_selected {
                    display_line.push(' ');
                }
                let selection = self.selection_bounds_for_line(index, display_line.chars().count());
                highlight_source_line(&display_line, index == self.cursor_row, palette, selection)
            })
            .collect::<Vec<_>>();
        Text::from(lines)
    }

    fn insert_char(&mut self, ch: char) {
        self.push_undo_state();
        self.delete_selection_raw();
        self.insert_char_raw(ch);
    }

    fn insert_char_raw(&mut self, ch: char) {
        self.follow_cursor = true;
        self.materialize_virtual_space();
        let cursor_col = self.cursor_col;
        let line = self.current_line_mut();
        let byte_index = char_to_byte_index(line, cursor_col);
        line.insert(byte_index, ch);
        self.cursor_col += 1;
        self.desired_col = self.cursor_col;
    }

    fn insert_spaces(&mut self, count: usize) {
        if count == 0 {
            return;
        }
        self.push_undo_state();
        self.delete_selection_raw();
        self.insert_str_raw(&" ".repeat(count));
    }

    fn insert_text(&mut self, text: &str) {
        if text.is_empty() {
            return;
        }
        self.push_undo_state();
        self.delete_selection_raw();
        self.insert_text_raw(text);
    }

    fn insert_text_raw(&mut self, text: &str) {
        self.follow_cursor = true;
        let normalized = text.replace("\r\n", "\n").replace('\r', "\n");
        let segments = normalized.split('\n').collect::<Vec<_>>();
        if segments.is_empty() {
            return;
        }

        if segments.len() == 1 {
            self.insert_str_raw(segments[0]);
            return;
        }

        let row = self.cursor_row;
        let col = self.cursor_col;
        self.materialize_virtual_space();
        let split_at = char_to_byte_index(&self.lines[row], col);
        let suffix = self.lines[row].split_off(split_at);
        self.lines[row].push_str(segments[0]);

        for (offset, segment) in segments.iter().enumerate().skip(1) {
            let is_last = offset == segments.len() - 1;
            let mut next_line = (*segment).to_string();
            if is_last {
                next_line.push_str(&suffix);
            }
            self.lines.insert(row + offset, next_line);
        }

        self.cursor_row = row + segments.len() - 1;
        self.cursor_col = segments
            .last()
            .map(|segment| segment.chars().count())
            .unwrap_or(0);
        self.desired_col = self.cursor_col;
    }

    fn insert_newline(&mut self) {
        self.push_undo_state();
        self.delete_selection_raw();
        self.insert_newline_raw();
    }

    fn insert_newline_raw(&mut self) {
        self.follow_cursor = true;
        self.materialize_virtual_space();
        let cursor_col = self.cursor_col;
        let current_row = self.cursor_row;
        let line = self.current_line_mut();
        let split_at = char_to_byte_index(line, cursor_col);
        let remainder = line.split_off(split_at);
        self.lines.insert(current_row + 1, remainder);
        self.cursor_row += 1;
        self.cursor_col = 0;
        self.desired_col = 0;
    }

    fn backspace(&mut self) {
        if self.has_selection() {
            self.push_undo_state();
            self.delete_selection_raw();
            return;
        }
        let current_len = line_char_len(self.current_line());
        if self.cursor_col > current_len {
            self.cursor_col -= 1;
            self.desired_col = self.cursor_col;
            return;
        }

        if self.cursor_col == 0 && self.cursor_row == 0 {
            return;
        }

        self.push_undo_state();
        self.backspace_raw();
    }

    fn backspace_raw(&mut self) {
        self.follow_cursor = true;
        if self.cursor_col > 0 {
            let cursor_col = self.cursor_col;
            let line = self.current_line_mut();
            let start = char_to_byte_index(line, cursor_col - 1);
            let end = char_to_byte_index(line, cursor_col);
            line.replace_range(start..end, "");
            self.cursor_col -= 1;
            self.desired_col = self.cursor_col;
            return;
        }

        let current = self.lines.remove(self.cursor_row);
        self.cursor_row -= 1;
        let previous_len = line_char_len(self.current_line());
        self.lines[self.cursor_row].push_str(&current);
        self.cursor_col = previous_len;
        self.desired_col = self.cursor_col;
    }

    fn delete(&mut self) {
        if self.has_selection() {
            self.push_undo_state();
            self.delete_selection_raw();
            return;
        }
        let current_len = line_char_len(self.current_line());
        if self.cursor_col > current_len {
            return;
        }

        if self.cursor_col >= current_len && self.cursor_row + 1 >= self.lines.len() {
            return;
        }

        self.push_undo_state();
        self.delete_raw();
    }

    fn delete_raw(&mut self) {
        self.follow_cursor = true;
        let current_len = line_char_len(self.current_line());
        if self.cursor_col < current_len {
            let cursor_col = self.cursor_col;
            let line = self.current_line_mut();
            let start = char_to_byte_index(line, cursor_col);
            let end = char_to_byte_index(line, cursor_col + 1);
            line.replace_range(start..end, "");
            return;
        }

        let next_line = self.lines.remove(self.cursor_row + 1);
        self.lines[self.cursor_row].push_str(&next_line);
    }

    fn move_left(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_start();
            return;
        }
        self.prepare_selection_for_move(selecting);
        if self.cursor_col > 0 {
            self.cursor_col -= 1;
        } else if self.cursor_row > 0 && !self.current_line().is_empty() {
            self.cursor_row -= 1;
            self.cursor_col = line_char_len(self.current_line());
        }
        self.desired_col = self.cursor_col;
        self.follow_cursor = true;
    }

    fn move_right(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_end();
            return;
        }
        self.prepare_selection_for_move(selecting);
        self.cursor_col += 1;
        self.desired_col = self.cursor_col;
        self.follow_cursor = true;
    }

    fn move_up(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_start();
            return;
        }
        self.prepare_selection_for_move(selecting);
        if self.cursor_row == 0 {
            return;
        }
        self.cursor_row -= 1;
        self.cursor_col = self.desired_col;
        self.follow_cursor = true;
    }

    fn move_down(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_end();
            return;
        }
        self.prepare_selection_for_move(selecting);
        if self.cursor_row + 1 >= self.lines.len() {
            self.lines.push(String::new());
        }
        self.cursor_row += 1;
        self.cursor_col = self.desired_col;
        self.follow_cursor = true;
    }

    fn move_home(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_start();
            return;
        }
        self.prepare_selection_for_move(selecting);
        self.cursor_col = 0;
        self.desired_col = 0;
        self.follow_cursor = true;
    }

    fn move_end(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_end();
            return;
        }
        self.prepare_selection_for_move(selecting);
        self.cursor_col = line_char_len(self.current_line());
        self.desired_col = self.cursor_col;
        self.follow_cursor = true;
    }

    fn page_up(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_start();
            return;
        }
        self.prepare_selection_for_move(selecting);
        self.cursor_row = self.cursor_row.saturating_sub(10);
        self.cursor_col = self.desired_col;
        self.follow_cursor = true;
    }

    fn page_down(&mut self, selecting: bool) {
        if !selecting && self.has_selection() {
            self.collapse_selection_to_end();
            return;
        }
        self.prepare_selection_for_move(selecting);
        let target_row = self.cursor_row + 10;
        while self.lines.len() <= target_row {
            self.lines.push(String::new());
        }
        self.cursor_row = target_row;
        self.cursor_col = self.desired_col;
        self.follow_cursor = true;
    }

    fn scroll_lines(&mut self, delta: isize) {
        self.follow_cursor = false;
        if delta < 0 {
            self.scroll_y = self.scroll_y.saturating_sub(delta.unsigned_abs());
        } else {
            self.scroll_y =
                (self.scroll_y + delta as usize).min(self.lines.len().saturating_sub(1));
        }
    }

    fn ensure_visible(&mut self, viewport_width: usize, viewport_height: usize) {
        if viewport_height == 0 || viewport_width == 0 {
            return;
        }

        if self.follow_cursor {
            if self.cursor_row < self.scroll_y {
                self.scroll_y = self.cursor_row;
            } else if self.cursor_row >= self.scroll_y + viewport_height {
                self.scroll_y = self.cursor_row + 1 - viewport_height;
            }
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

    fn cursor_point(&self) -> CursorPoint {
        CursorPoint {
            row: self.cursor_row,
            col: self.cursor_col,
        }
    }

    fn prepare_selection_for_move(&mut self, selecting: bool) {
        if selecting {
            self.selection_anchor.get_or_insert(self.cursor_point());
        } else {
            self.clear_selection();
        }
    }

    fn start_mouse_selection(&mut self, point: CursorPoint) {
        self.move_cursor_to(point, false);
        self.selection_anchor = Some(point);
        self.mouse_selecting = true;
    }

    fn drag_mouse_selection(&mut self, point: CursorPoint) {
        if !self.mouse_selecting {
            self.start_mouse_selection(point);
            return;
        }
        self.move_cursor_to(point, true);
    }

    fn finish_mouse_selection(&mut self) {
        self.mouse_selecting = false;
        if self.selection_anchor == Some(self.cursor_point()) {
            self.selection_anchor = None;
        }
    }

    fn move_cursor_to(&mut self, point: CursorPoint, selecting: bool) {
        if selecting {
            self.selection_anchor.get_or_insert(self.cursor_point());
        } else if !self.mouse_selecting {
            self.clear_selection();
        }
        while self.lines.len() <= point.row {
            self.lines.push(String::new());
        }
        self.cursor_row = point.row;
        self.cursor_col = point.col;
        self.desired_col = point.col;
        self.follow_cursor = true;
    }

    fn collapse_selection_to_start(&mut self) {
        if let Some(selection) = self.selection_range() {
            self.cursor_row = selection.start.row;
            self.cursor_col = selection.start.col;
            self.desired_col = self.cursor_col;
            self.clear_selection();
            self.follow_cursor = true;
        }
    }

    fn collapse_selection_to_end(&mut self) {
        if let Some(selection) = self.selection_range() {
            self.cursor_row = selection.end.row;
            self.cursor_col = selection.end.col;
            self.desired_col = self.cursor_col;
            self.clear_selection();
            self.follow_cursor = true;
        }
    }

    fn select_word_at(&mut self, point: CursorPoint) {
        while self.lines.len() <= point.row {
            self.lines.push(String::new());
        }
        let line = self.line_for_row(point.row);
        let chars = line.chars().collect::<Vec<_>>();
        if chars.is_empty() {
            self.select_line_at(point);
            return;
        }
        let pivot = point.col.min(chars.len().saturating_sub(1));
        let is_word = is_word_char(chars[pivot]);
        let mut start = pivot;
        while start > 0 && is_same_word_class(chars[start - 1], is_word) {
            start -= 1;
        }
        let mut end = pivot;
        while end < chars.len() && is_same_word_class(chars[end], is_word) {
            end += 1;
        }
        self.selection_anchor = Some(CursorPoint {
            row: point.row,
            col: start,
        });
        self.cursor_row = point.row;
        self.cursor_col = end;
        self.desired_col = end;
        self.mouse_selecting = false;
        self.follow_cursor = true;
    }

    fn select_line_at(&mut self, point: CursorPoint) {
        while self.lines.len() <= point.row {
            self.lines.push(String::new());
        }
        let end = line_char_len(self.line_for_row(point.row));
        self.selection_anchor = Some(CursorPoint {
            row: point.row,
            col: 0,
        });
        self.cursor_row = point.row;
        self.cursor_col = end;
        self.desired_col = end;
        self.mouse_selecting = false;
        self.follow_cursor = true;
    }

    fn current_block_text(&self) -> Option<String> {
        if self.lines.is_empty() {
            return None;
        }

        let mut pivot = self.cursor_row.min(self.lines.len() - 1);
        if self.line_for_row(pivot).trim().is_empty() {
            if let Some(next) =
                (pivot + 1..self.lines.len()).find(|row| !self.line_for_row(*row).trim().is_empty())
            {
                pivot = next;
            } else if let Some(prev) =
                (0..pivot).rfind(|row| !self.line_for_row(*row).trim().is_empty())
            {
                pivot = prev;
            } else {
                return None;
            }
        }

        let mut start = pivot;
        while start > 0 && !self.line_for_row(start - 1).trim().is_empty() {
            start -= 1;
        }

        let mut end = pivot;
        while end + 1 < self.lines.len() && !self.line_for_row(end + 1).trim().is_empty() {
            end += 1;
        }

        Some(self.lines[start..=end].join("\n"))
    }

    fn current_line_text(&self) -> Option<String> {
        let line = self.line_for_row(self.cursor_row);
        if line.trim().is_empty() {
            None
        } else {
            Some(line.to_string())
        }
    }

    fn current_function_text(&self) -> Option<String> {
        if self.lines.is_empty() {
            return None;
        }

        let start = (0..=self.cursor_row.min(self.lines.len().saturating_sub(1)))
            .rev()
            .find(|row| looks_like_function_signature(self.line_for_row(*row)))?;

        let base_indent = leading_space_count(self.line_for_row(start));
        let mut end = start;
        for row in start + 1..self.lines.len() {
            let line = self.line_for_row(row);
            if line.trim().is_empty() {
                end = row;
                continue;
            }
            if leading_space_count(line) <= base_indent {
                break;
            }
            end = row;
        }

        Some(self.lines[start..=end].join("\n").trim_end().to_string())
    }

    fn selection_or_current_block(&self) -> Option<RunSlice> {
        if let Some(selection) = self.selected_text() {
            if !selection.trim().is_empty() {
                return Some(RunSlice::Selection(selection));
            }
        }
        self.current_block_text().map(RunSlice::Block)
    }

    fn selection_range(&self) -> Option<SelectionRange> {
        let anchor = self.selection_anchor?;
        let cursor = self.cursor_point();
        if anchor == cursor {
            None
        } else {
            Some(
                SelectionRange {
                    start: anchor,
                    end: cursor,
                }
                .normalize(),
            )
        }
    }

    fn selection_bounds_for_line(
        &self,
        line_index: usize,
        display_len: usize,
    ) -> Option<(usize, usize)> {
        let selection = self.selection_range()?;
        if line_index < selection.start.row || line_index > selection.end.row {
            return None;
        }

        let start = if line_index == selection.start.row {
            selection.start.col.min(display_len)
        } else {
            0
        };
        let mut end = if line_index == selection.end.row {
            selection.end.col.min(display_len)
        } else {
            display_len
        };

        if display_len == 1 && self.line_for_row(line_index).is_empty() && start == 0 && end == 0 {
            end = 1;
        }

        if start >= end {
            None
        } else {
            Some((start, end))
        }
    }

    fn insert_str_raw(&mut self, text: &str) {
        self.materialize_virtual_space();
        let cursor_col = self.cursor_col;
        let line = self.current_line_mut();
        let byte_index = char_to_byte_index(line, cursor_col);
        line.insert_str(byte_index, text);
        self.cursor_col += text.chars().count();
        self.desired_col = self.cursor_col;
    }

    fn materialize_virtual_space(&mut self) {
        let line_len = line_char_len(self.current_line());
        if self.cursor_col <= line_len {
            return;
        }
        let padding = self.cursor_col - line_len;
        self.current_line_mut().push_str(&" ".repeat(padding));
    }

    fn delete_selection_raw(&mut self) -> bool {
        let Some(selection) = self.selection_range() else {
            return false;
        };

        let start_row = selection.start.row;
        let start_col = selection.start.col;
        let end_row = selection.end.row;
        let end_col = selection.end.col;

        if start_row == end_row {
            let line = &mut self.lines[start_row];
            let start_byte = char_to_byte_index(line, start_col.min(line_char_len(line)));
            let end_byte = char_to_byte_index(line, end_col.min(line_char_len(line)));
            line.replace_range(start_byte..end_byte, "");
        } else {
            let prefix = {
                let line = &self.lines[start_row];
                let start_byte = char_to_byte_index(line, start_col.min(line_char_len(line)));
                line[..start_byte].to_string()
            };
            let suffix = {
                let line = &self.lines[end_row];
                let end_byte = char_to_byte_index(line, end_col.min(line_char_len(line)));
                line[end_byte..].to_string()
            };
            self.lines
                .splice(start_row..=end_row, [format!("{prefix}{suffix}")]);
        }

        self.cursor_row = start_row;
        self.cursor_col = start_col;
        self.desired_col = start_col;
        self.clear_selection();
        true
    }

    fn push_undo_state(&mut self) {
        self.undo_stack.push(EditorSnapshot {
            lines: self.lines.clone(),
            cursor_row: self.cursor_row,
            cursor_col: self.cursor_col,
            desired_col: self.desired_col,
            scroll_x: self.scroll_x,
            scroll_y: self.scroll_y,
            follow_cursor: self.follow_cursor,
            selection_anchor: self.selection_anchor,
        });
        self.redo_stack.clear();
        if self.undo_stack.len() > UNDO_HISTORY_LIMIT {
            let drain = self.undo_stack.len() - UNDO_HISTORY_LIMIT;
            self.undo_stack.drain(0..drain);
        }
    }

    fn undo(&mut self) -> bool {
        let Some(snapshot) = self.undo_stack.pop() else {
            return false;
        };
        self.redo_stack.push(EditorSnapshot {
            lines: self.lines.clone(),
            cursor_row: self.cursor_row,
            cursor_col: self.cursor_col,
            desired_col: self.desired_col,
            scroll_x: self.scroll_x,
            scroll_y: self.scroll_y,
            follow_cursor: self.follow_cursor,
            selection_anchor: self.selection_anchor,
        });
        self.lines = snapshot.lines;
        self.cursor_row = snapshot.cursor_row;
        self.cursor_col = snapshot.cursor_col;
        self.desired_col = snapshot.desired_col;
        self.scroll_x = snapshot.scroll_x;
        self.scroll_y = snapshot.scroll_y;
        self.follow_cursor = snapshot.follow_cursor;
        self.selection_anchor = snapshot.selection_anchor;
        self.mouse_selecting = false;
        true
    }

    fn redo(&mut self) -> bool {
        let Some(snapshot) = self.redo_stack.pop() else {
            return false;
        };
        self.undo_stack.push(EditorSnapshot {
            lines: self.lines.clone(),
            cursor_row: self.cursor_row,
            cursor_col: self.cursor_col,
            desired_col: self.desired_col,
            scroll_x: self.scroll_x,
            scroll_y: self.scroll_y,
            follow_cursor: self.follow_cursor,
            selection_anchor: self.selection_anchor,
        });
        self.lines = snapshot.lines;
        self.cursor_row = snapshot.cursor_row;
        self.cursor_col = snapshot.cursor_col;
        self.desired_col = snapshot.desired_col;
        self.scroll_x = snapshot.scroll_x;
        self.scroll_y = snapshot.scroll_y;
        self.follow_cursor = snapshot.follow_cursor;
        self.selection_anchor = snapshot.selection_anchor;
        self.mouse_selecting = false;
        true
    }
}

#[derive(Debug, Clone)]
struct EditorSnapshot {
    lines: Vec<String>,
    cursor_row: usize,
    cursor_col: usize,
    desired_col: usize,
    scroll_x: usize,
    scroll_y: usize,
    follow_cursor: bool,
    selection_anchor: Option<CursorPoint>,
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

fn visual_width_to_cursor_col(line: &str, visual_width: usize) -> usize {
    let mut consumed_width = 0usize;
    let mut col = 0usize;
    for ch in line.chars() {
        let ch_width = UnicodeWidthStr::width(ch.encode_utf8(&mut [0; 4]));
        if consumed_width + ch_width > visual_width {
            return col;
        }
        consumed_width += ch_width;
        col += 1;
    }
    col + visual_width.saturating_sub(consumed_width)
}

fn cursor_prefix_width(line: &str, cursor_col: usize) -> usize {
    let line_len = line_char_len(line);
    let byte_index = char_to_byte_index(line, cursor_col);
    UnicodeWidthStr::width(&line[..byte_index]) + cursor_col.saturating_sub(line_len)
}

fn is_word_char(ch: char) -> bool {
    ch.is_ascii_alphanumeric() || ch == '_'
}

fn is_same_word_class(ch: char, word_class: bool) -> bool {
    if word_class {
        is_word_char(ch)
    } else {
        !ch.is_whitespace() && !is_word_char(ch)
    }
}

fn leading_space_count(line: &str) -> usize {
    line.chars().take_while(|ch| ch.is_whitespace()).count()
}

fn looks_like_function_signature(line: &str) -> bool {
    let trimmed = line.trim_start();
    trimmed.starts_with("fn ")
        || trimmed.starts_with("pub fn ")
        || trimmed.starts_with("async fn ")
        || trimmed.starts_with("pub async fn ")
}

fn short_file_chip_label(path: &str) -> String {
    let path_buf = PathBuf::from(path);
    let file_name = path_buf
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or(path);
    const MAX_LABEL: usize = 18;
    if file_name.chars().count() <= MAX_LABEL {
        file_name.to_string()
    } else {
        let tail = file_name
            .chars()
            .rev()
            .take(MAX_LABEL - 1)
            .collect::<Vec<_>>()
            .into_iter()
            .rev()
            .collect::<String>();
        format!("…{tail}")
    }
}

fn slice_chars(text: &str, start: usize, len: usize) -> String {
    text.chars().skip(start).take(len).collect()
}

fn styled_text_line(text: &str, style: Style, selection: Option<(usize, usize)>) -> Line<'static> {
    let mut spans = Vec::new();
    push_selected_text_spans(&mut spans, text, style, selection);
    Line::from(spans)
}

fn push_selected_text_spans(
    spans: &mut Vec<Span<'static>>,
    text: &str,
    style: Style,
    selection: Option<(usize, usize)>,
) {
    match selection {
        Some((sel_start, sel_end)) if sel_start < sel_end => {
            let total_len = text.chars().count();
            let prefix_len = sel_start.min(total_len);
            let selected_len = sel_end.min(total_len).saturating_sub(prefix_len);
            if prefix_len > 0 {
                spans.push(Span::styled(slice_chars(text, 0, prefix_len), style));
            }
            if selected_len > 0 {
                spans.push(Span::styled(
                    slice_chars(text, prefix_len, selected_len),
                    style.add_modifier(Modifier::REVERSED),
                ));
            }
            if prefix_len + selected_len < total_len {
                spans.push(Span::styled(
                    slice_chars(
                        text,
                        prefix_len + selected_len,
                        total_len - prefix_len - selected_len,
                    ),
                    style,
                ));
            }
            if total_len == 0 {
                spans.push(Span::styled(String::new(), style));
            }
        }
        _ => spans.push(Span::styled(text.to_string(), style)),
    }
}

fn output_line_style(index: usize, entries: &[OutputEntry], palette: ReplPalette) -> Style {
    if entries.is_empty() {
        return palette.muted_style();
    }

    let mut cursor = 0usize;
    for entry in entries {
        if index == cursor {
            return entry.kind.title_style(palette);
        }
        cursor += 1;
        for _ in &entry.body {
            if index == cursor {
                return entry.kind.body_style(palette);
            }
            cursor += 1;
        }
        if index == cursor {
            return Style::default().fg(palette.text_muted);
        }
        cursor += 1;
    }
    Style::default().fg(palette.text_primary)
}

fn output_selection_bounds_for_line(
    selection: SelectionRange,
    line_index: usize,
    display_len: usize,
) -> Option<(usize, usize)> {
    if line_index < selection.start.row || line_index > selection.end.row {
        return None;
    }
    let start = if line_index == selection.start.row {
        selection.start.col.min(display_len)
    } else {
        0
    };
    let end = if line_index == selection.end.row {
        selection.end.col.min(display_len)
    } else {
        display_len
    };
    if start < end {
        Some((start, end))
    } else {
        None
    }
}

fn output_scroll_y(total_lines: usize, viewport_height: usize) -> usize {
    total_lines.saturating_sub(viewport_height)
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
        editor.move_home(false);
        editor.backspace();

        assert_eq!(editor.line_count(), 1);
        assert_eq!(editor.buffer(), "fn main()    return 7");
    }

    #[test]
    fn move_down_materializes_missing_empty_line() {
        let mut editor = ReplEditor::default();
        editor.move_down(false);
        assert_eq!(editor.line_count(), 2);
        assert_eq!(editor.cursor_row, 1);
    }

    #[test]
    fn moving_across_empty_lines_preserves_virtual_column() {
        let mut editor = ReplEditor::default();
        editor.insert_text("abcdef");
        editor.insert_newline();
        editor.insert_newline();
        editor.cursor_row = 0;
        editor.cursor_col = 5;
        editor.desired_col = 5;

        editor.move_down(false);
        assert_eq!(editor.cursor_row, 1);
        assert_eq!(editor.cursor_col, 5);

        editor.move_down(false);
        assert_eq!(editor.cursor_row, 2);
        assert_eq!(editor.cursor_col, 5);
    }

    #[test]
    fn move_left_does_not_jump_out_of_empty_line_origin() {
        let mut editor = ReplEditor::default();
        editor.move_down(false);
        editor.move_right(false);
        editor.move_right(false);

        editor.move_left(false);
        assert_eq!(editor.cursor_row, 1);
        assert_eq!(editor.cursor_col, 1);

        editor.move_left(false);
        assert_eq!(editor.cursor_row, 1);
        assert_eq!(editor.cursor_col, 0);

        editor.move_left(false);
        assert_eq!(editor.cursor_row, 1);
        assert_eq!(editor.cursor_col, 0);
    }

    #[test]
    fn selected_text_spans_multiple_lines() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("alpha\nbeta\ngamma");
        editor.selection_anchor = Some(CursorPoint { row: 0, col: 2 });
        editor.cursor_row = 2;
        editor.cursor_col = 2;
        assert_eq!(editor.selected_text().as_deref(), Some("pha\nbeta\nga"));
    }

    #[test]
    fn left_arrow_collapses_selection_to_start() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("alpha beta");
        editor.selection_anchor = Some(CursorPoint { row: 0, col: 2 });
        editor.cursor_row = 0;
        editor.cursor_col = 7;

        editor.move_left(false);

        assert_eq!(editor.cursor_row, 0);
        assert_eq!(editor.cursor_col, 2);
        assert!(!editor.has_selection());
    }

    #[test]
    fn right_arrow_collapses_selection_to_end() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("alpha beta");
        editor.selection_anchor = Some(CursorPoint { row: 0, col: 2 });
        editor.cursor_row = 0;
        editor.cursor_col = 7;

        editor.move_right(false);

        assert_eq!(editor.cursor_row, 0);
        assert_eq!(editor.cursor_col, 7);
        assert!(!editor.has_selection());
    }

    #[test]
    fn wheel_scroll_stays_decoupled_from_cursor_until_cursor_moves() {
        let mut editor = ReplEditor::default();
        editor.lines = vec![String::new(); 120];
        editor.cursor_row = 10;
        editor.scroll_y = 10;

        editor.scroll_lines(40);
        editor.ensure_visible(80, 20);
        assert_eq!(editor.scroll_y, 50);

        editor.move_down(false);
        editor.ensure_visible(80, 20);
        assert_eq!(editor.scroll_y, 11);
    }

    #[test]
    fn current_block_extracts_nearest_non_empty_block() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("use std::fs\n\nfn main() -> Int:\n    let value = 7\n    return value\n\nfn other() -> Int:\n    return 9");
        editor.cursor_row = 3;
        editor.cursor_col = 4;

        assert_eq!(
            editor.current_block_text().as_deref(),
            Some("fn main() -> Int:\n    let value = 7\n    return value")
        );
    }

    #[test]
    fn current_line_extracts_non_empty_line() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("alpha\nbeta");
        editor.cursor_row = 1;
        assert_eq!(editor.current_line_text().as_deref(), Some("beta"));
    }

    #[test]
    fn load_text_at_start_opens_at_top_left() {
        let mut editor = ReplEditor::default();
        editor.load_text_at_start("alpha\nbeta\ngamma");

        assert_eq!(editor.cursor_row, 0);
        assert_eq!(editor.cursor_col, 0);
        assert_eq!(editor.scroll_x, 0);
        assert_eq!(editor.scroll_y, 0);
    }

    #[test]
    fn current_function_extracts_indented_function_body() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw(
            "use std::fs\n\nfn alpha() -> Int:\n    let seed = 7\n    return seed\n\nfn beta() -> Int:\n    return 9",
        );
        editor.cursor_row = 4;
        editor.cursor_col = 3;

        assert_eq!(
            editor.current_function_text().as_deref(),
            Some("fn alpha() -> Int:\n    let seed = 7\n    return seed")
        );
    }

    #[test]
    fn selection_run_slice_wins_over_block_run() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("alpha\nbeta\ngamma");
        editor.selection_anchor = Some(CursorPoint { row: 0, col: 1 });
        editor.cursor_row = 1;
        editor.cursor_col = 2;

        assert_eq!(
            editor.selection_or_current_block(),
            Some(RunSlice::Selection("lpha\nbe".to_string()))
        );
    }

    #[test]
    fn double_click_word_selection_grabs_identifier() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("entangle world.signal");

        editor.select_word_at(CursorPoint { row: 0, col: 10 });

        assert_eq!(editor.selected_text().as_deref(), Some("world"));
    }

    #[test]
    fn triple_click_line_selection_grabs_whole_line() {
        let mut editor = ReplEditor::default();
        editor.set_text_raw("entangle world.signal");

        editor.select_line_at(CursorPoint { row: 0, col: 4 });

        assert_eq!(
            editor.selected_text().as_deref(),
            Some("entangle world.signal")
        );
    }

    #[test]
    fn undo_restores_previous_buffer_state() {
        let mut editor = ReplEditor::default();
        editor.insert_text("abc");
        editor.undo();
        assert_eq!(editor.buffer(), "");
    }

    #[test]
    fn redo_restores_undone_buffer_state() {
        let mut editor = ReplEditor::default();
        editor.insert_text("abc");
        editor.undo();
        assert!(editor.redo());
        assert_eq!(editor.buffer(), "abc");
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

    #[test]
    fn open_directive_is_routed_to_open_submission() {
        match collect_submission(".open demo.kn") {
            Submission::Open(Some(path)) => assert_eq!(path, "demo.kn"),
            other => panic!("expected open submission, got {other:?}"),
        }
    }

    #[test]
    fn theme_cycle_wraps_from_plain_to_last_palette() {
        let mut app = ReplApp::new(ReplTerminalConfig::default());
        app.theme_name = "plain".to_string();
        app.cycle_theme(true);
        assert_eq!(app.theme_name, "sandstone");
    }

    #[test]
    fn successful_runs_keep_buffer_live() {
        let mut app = ReplApp::new(ReplTerminalConfig::default());
        app.evaluator = ReplEvaluator::interpret_only_for_testing();
        app.editor.set_text("fn main() -> Int:\n    return 7");
        let expected = app.editor.buffer();
        app.run_current_buffer();
        assert_eq!(app.editor.buffer(), expected);
    }

    #[test]
    fn gutter_width_is_stable_across_double_and_triple_digits() {
        let mut editor = ReplEditor::default();
        editor.lines = vec![String::new(); 9];
        let single = editor.gutter_width();
        editor.lines = vec![String::new(); 99];
        let double = editor.gutter_width();
        editor.lines = vec![String::new(); 999];
        let triple = editor.gutter_width();
        assert_eq!(single, double);
        assert_eq!(double, triple);
    }

    #[test]
    fn track_open_file_keeps_recent_order_without_duplicates() {
        let mut app = ReplApp::new(ReplTerminalConfig::default());
        app.track_open_file("X:/demo/alpha.kn");
        app.track_open_file("X:/demo/beta.kn");
        app.track_open_file("X:/demo/alpha.kn");

        assert_eq!(
            app.recent_files,
            vec![
                "X:/demo/alpha.kn".to_string(),
                "X:/demo/beta.kn".to_string()
            ]
        );
    }

    #[test]
    fn selected_output_text_spans_multiple_lines() {
        let mut app = ReplApp::new(ReplTerminalConfig::default());
        app.push_output(
            OutputKind::Error,
            "Run #1",
            vec!["first problem".to_string(), "second problem".to_string()],
        );
        app.output_selection_anchor = Some(CursorPoint { row: 1, col: 2 });
        app.output_selection_cursor = Some(CursorPoint { row: 2, col: 6 });

        assert_eq!(
            app.selected_output_text().as_deref(),
            Some("first problem\n  seco")
        );
    }
}
