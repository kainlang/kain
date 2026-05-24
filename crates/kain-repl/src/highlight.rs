use kain_core::lexer::TokenKind;
use logos::Logos;
use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};

use crate::command::ReplDirective;

pub fn highlight_source_line(source: &str, is_current_line: bool) -> Line<'static> {
    if let Some(line) = highlight_directive_line(source, is_current_line) {
        return line;
    }

    let base_style = base_line_style(is_current_line);
    let mut spans = Vec::new();
    let mut lexer = TokenKind::lexer(source);
    let mut cursor = 0usize;
    let mut highlight_next_attribute_name = false;

    while let Some(result) = lexer.next() {
        let span = lexer.span();
        if span.start > cursor {
            spans.push(Span::styled(source[cursor..span.start].to_string(), base_style));
        }

        let lexeme = &source[span.start..span.end];
        let style = match result {
            Ok(ref kind) => {
                token_style(kind, lexeme, highlight_next_attribute_name).patch(base_style)
            }
            Err(_) => invalid_token_style().patch(base_style),
        };
        spans.push(Span::styled(lexeme.to_string(), style));

        highlight_next_attribute_name = matches!(result, Ok(TokenKind::At));
        cursor = span.end;
    }

    if cursor < source.len() {
        spans.push(Span::styled(source[cursor..].to_string(), base_style));
    }

    if spans.is_empty() {
        spans.push(Span::styled(String::new(), base_style));
    }

    Line::from(spans)
}

fn highlight_directive_line(source: &str, is_current_line: bool) -> Option<Line<'static>> {
    let trimmed = source.trim();
    if ReplDirective::parse(trimmed).is_none() {
        return None;
    }

    let base_style = base_line_style(is_current_line);
    Some(Line::from(vec![Span::styled(
        source.to_string(),
        directive_style().patch(base_style),
    )]))
}

fn base_line_style(is_current_line: bool) -> Style {
    let mut style = Style::default().fg(Color::Rgb(230, 224, 208));
    if is_current_line {
        style = style.bg(Color::Rgb(24, 33, 45));
    }
    style
}

fn directive_style() -> Style {
    Style::default()
        .fg(Color::Rgb(255, 206, 86))
        .add_modifier(Modifier::BOLD)
}

fn invalid_token_style() -> Style {
    Style::default()
        .fg(Color::Rgb(255, 89, 168))
        .add_modifier(Modifier::UNDERLINED)
}

fn token_style(kind: &TokenKind, lexeme: &str, attribute_name: bool) -> Style {
    if attribute_name {
        return Style::default()
            .fg(Color::Rgb(255, 206, 86))
            .add_modifier(Modifier::BOLD);
    }

    match kind {
        TokenKind::Comment | TokenKind::HashComment => Style::default()
            .fg(Color::Rgb(124, 137, 154))
            .add_modifier(Modifier::ITALIC),
        TokenKind::Int(_) | TokenKind::Float(_) => Style::default().fg(Color::Rgb(255, 140, 105)),
        TokenKind::String(_) | TokenKind::FString(_) | TokenKind::Char(_) => {
            Style::default().fg(Color::Rgb(171, 255, 118))
        }
        TokenKind::True | TokenKind::False | TokenKind::None => Style::default()
            .fg(Color::Rgb(127, 195, 255))
            .add_modifier(Modifier::BOLD),
        TokenKind::Ident(name) if looks_like_type_name(name) => Style::default()
            .fg(Color::Rgb(127, 195, 255))
            .add_modifier(Modifier::BOLD),
        TokenKind::Ident(_) => Style::default().fg(Color::Rgb(230, 224, 208)),
        TokenKind::At => Style::default()
            .fg(Color::Rgb(255, 206, 86))
            .add_modifier(Modifier::BOLD),
        kind if is_keyword(kind) => Style::default()
            .fg(Color::Rgb(255, 179, 71))
            .add_modifier(Modifier::BOLD),
        kind if is_kain_semantic_keyword(kind) => Style::default()
            .fg(Color::Rgb(92, 225, 230))
            .add_modifier(Modifier::BOLD),
        kind if is_operator_or_punctuation(kind) => {
            Style::default().fg(Color::Rgb(214, 188, 139))
        }
        _ if lexeme.starts_with('@') => directive_style(),
        _ => Style::default().fg(Color::Rgb(230, 224, 208)),
    }
}

fn looks_like_type_name(name: &str) -> bool {
    name.chars()
        .next()
        .map(|ch| ch.is_ascii_uppercase())
        .unwrap_or(false)
}

fn is_keyword(kind: &TokenKind) -> bool {
    matches!(
        kind,
        TokenKind::Fn
            | TokenKind::Let
            | TokenKind::Mut
            | TokenKind::Var
            | TokenKind::Const
            | TokenKind::If
            | TokenKind::Else
            | TokenKind::Elif
            | TokenKind::Match
            | TokenKind::For
            | TokenKind::While
            | TokenKind::Loop
            | TokenKind::Break
            | TokenKind::Continue
            | TokenKind::Return
            | TokenKind::Await
            | TokenKind::In
            | TokenKind::With
            | TokenKind::As
            | TokenKind::TypeKw
            | TokenKind::Struct
            | TokenKind::Enum
            | TokenKind::Trait
            | TokenKind::Impl
            | TokenKind::Pub
            | TokenKind::Mod
            | TokenKind::Use
            | TokenKind::SelfLower
            | TokenKind::SelfUpper
            | TokenKind::Pure
            | TokenKind::Io
            | TokenKind::AsyncKw
            | TokenKind::Async
            | TokenKind::Gpu
            | TokenKind::Reactive
            | TokenKind::Unsafe
    )
}

fn is_kain_semantic_keyword(kind: &TokenKind) -> bool {
    matches!(
        kind,
        TokenKind::Component
            | TokenKind::Shader
            | TokenKind::Actor
            | TokenKind::State
            | TokenKind::Spawn
            | TokenKind::Send
            | TokenKind::Receive
            | TokenKind::Emit
            | TokenKind::Comptime
            | TokenKind::Macro
            | TokenKind::Vertex
            | TokenKind::Fragment
            | TokenKind::Collapse
            | TokenKind::Observe
            | TokenKind::Decay
            | TokenKind::Share
            | TokenKind::Fanout
            | TokenKind::Test
    )
}

fn is_operator_or_punctuation(kind: &TokenKind) -> bool {
    matches!(
        kind,
        TokenKind::Plus
            | TokenKind::Minus
            | TokenKind::Star
            | TokenKind::Slash
            | TokenKind::Percent
            | TokenKind::Power
            | TokenKind::EqEq
            | TokenKind::NotEq
            | TokenKind::Lt
            | TokenKind::Gt
            | TokenKind::LtEq
            | TokenKind::GtEq
            | TokenKind::And
            | TokenKind::Or
            | TokenKind::Not
            | TokenKind::Amp
            | TokenKind::Pipe
            | TokenKind::Caret
            | TokenKind::Tilde
            | TokenKind::Shl
            | TokenKind::Shr
            | TokenKind::Eq
            | TokenKind::PlusEq
            | TokenKind::MinusEq
            | TokenKind::StarEq
            | TokenKind::SlashEq
            | TokenKind::PercentEq
            | TokenKind::AmpEq
            | TokenKind::PipeEq
            | TokenKind::CaretEq
            | TokenKind::ShlEq
            | TokenKind::ShrEq
            | TokenKind::LParen
            | TokenKind::RParen
            | TokenKind::LBracket
            | TokenKind::RBracket
            | TokenKind::LBrace
            | TokenKind::RBrace
            | TokenKind::Comma
            | TokenKind::Dot
            | TokenKind::DotDot
            | TokenKind::DotDotDot
            | TokenKind::Colon
            | TokenKind::ColonColon
            | TokenKind::Semi
            | TokenKind::Arrow
            | TokenKind::FatArrow
            | TokenKind::QuestionQuestion
            | TokenKind::QuestionDot
            | TokenKind::Question
            | TokenKind::LtSlash
    )
}
