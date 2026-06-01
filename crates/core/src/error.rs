pub use kain_error::error::*;

/// Convert a TokenKind to user-friendly string representation.
/// This replaces debug formatting with readable syntax in parser diagnostics.
pub fn token_kind_to_user_string(kind: &crate::lexer::TokenKind) -> String {
    use crate::lexer::TokenKind;
    match kind {
        // Keywords
        TokenKind::Fn => "keyword 'fn'".to_string(),
        TokenKind::Let => "keyword 'let'".to_string(),
        TokenKind::Mut => "keyword 'mut'".to_string(),
        TokenKind::Var => "keyword 'var'".to_string(),
        TokenKind::Const => "keyword 'const'".to_string(),
        TokenKind::If => "keyword 'if'".to_string(),
        TokenKind::Else => "keyword 'else'".to_string(),
        TokenKind::Elif => "keyword 'elif'".to_string(),
        TokenKind::Match => "keyword 'match'".to_string(),
        TokenKind::For => "keyword 'for'".to_string(),
        TokenKind::While => "keyword 'while'".to_string(),
        TokenKind::Loop => "keyword 'loop'".to_string(),
        TokenKind::Break => "keyword 'break'".to_string(),
        TokenKind::Continue => "keyword 'continue'".to_string(),
        TokenKind::Defer => "keyword 'defer'".to_string(),
        TokenKind::Return => "keyword 'return'".to_string(),
        TokenKind::Await => "keyword 'await'".to_string(),
        TokenKind::In => "keyword 'in'".to_string(),
        TokenKind::With => "keyword 'with'".to_string(),
        TokenKind::As => "keyword 'as'".to_string(),
        TokenKind::TypeKw => "keyword 'type'".to_string(),
        TokenKind::Struct => "keyword 'struct'".to_string(),
        TokenKind::Enum => "keyword 'enum'".to_string(),
        TokenKind::Trait => "keyword 'trait'".to_string(),
        TokenKind::Impl => "keyword 'impl'".to_string(),
        TokenKind::Pub => "keyword 'pub'".to_string(),
        TokenKind::Mod => "keyword 'mod'".to_string(),
        TokenKind::Use => "keyword 'use'".to_string(),
        TokenKind::SelfLower => "keyword 'self'".to_string(),
        TokenKind::SelfUpper => "keyword 'Self'".to_string(),
        TokenKind::True => "keyword 'true'".to_string(),
        TokenKind::False => "keyword 'false'".to_string(),
        TokenKind::None => "keyword 'none'".to_string(),

        // Special keywords
        TokenKind::Component => "keyword 'component'".to_string(),
        TokenKind::Shader => "keyword 'shader'".to_string(),
        TokenKind::Actor => "keyword 'actor'".to_string(),
        TokenKind::State => "keyword 'state'".to_string(),
        TokenKind::Spawn => "keyword 'spawn'".to_string(),
        TokenKind::Send => "keyword 'send'".to_string(),
        TokenKind::Receive => "keyword 'receive'".to_string(),
        TokenKind::Emit => "keyword 'emit'".to_string(),
        TokenKind::Comptime => "keyword 'comptime'".to_string(),
        TokenKind::Macro => "keyword 'macro'".to_string(),
        TokenKind::Vertex => "keyword 'vertex'".to_string(),
        TokenKind::Fragment => "keyword 'fragment'".to_string(),
        TokenKind::Collapse => "keyword 'collapse'".to_string(),
        TokenKind::Observe => "keyword 'observe'".to_string(),
        TokenKind::Decay => "keyword 'decay'".to_string(),
        TokenKind::Share => "keyword 'share'".to_string(),
        TokenKind::Fanout => "keyword 'fanout'".to_string(),
        TokenKind::Test => "keyword 'test'".to_string(),

        // Effect keywords
        TokenKind::Pure => "keyword 'Pure'".to_string(),
        TokenKind::Io => "keyword 'IO'".to_string(),
        TokenKind::AsyncKw => "keyword 'async'".to_string(),
        TokenKind::Async => "keyword 'Async'".to_string(),
        TokenKind::Gpu => "keyword 'GPU'".to_string(),
        TokenKind::Reactive => "keyword 'Reactive'".to_string(),
        TokenKind::Unsafe => "keyword 'Unsafe'".to_string(),

        // Literals
        TokenKind::Int(n) => format!("number {}", n),
        TokenKind::Float(f) => format!("number {}", f),
        TokenKind::String(s) => format!("string \"{}\"", s),
        TokenKind::FString(s) => format!("f-string f\"{}\"", s),
        TokenKind::Char(c) => format!("character '{}'", c),
        TokenKind::Ident(name) => format!("identifier '{}'", name),

        // Operators
        TokenKind::PlusPlus => "'++'".to_string(),
        TokenKind::MinusMinus => "'--'".to_string(),
        TokenKind::Plus => "'+'".to_string(),
        TokenKind::Minus => "'-'".to_string(),
        TokenKind::Star => "'*'".to_string(),
        TokenKind::Slash => "'/'".to_string(),
        TokenKind::Percent => "'%'".to_string(),
        TokenKind::Power => "'**'".to_string(),
        TokenKind::EqEq => "'=='".to_string(),
        TokenKind::NotEq => "'!='".to_string(),
        TokenKind::Lt => "'<'".to_string(),
        TokenKind::Gt => "'>'".to_string(),
        TokenKind::LtEq => "'<='".to_string(),
        TokenKind::GtEq => "'>='".to_string(),
        TokenKind::And => "'&&' or 'and'".to_string(),
        TokenKind::Or => "'||' or 'or'".to_string(),
        TokenKind::Not => "'!'".to_string(),
        TokenKind::Amp => "'&'".to_string(),
        TokenKind::Pipe => "'|'".to_string(),
        TokenKind::Caret => "'^'".to_string(),
        TokenKind::Tilde => "'~'".to_string(),
        TokenKind::Shl => "'<<'".to_string(),
        TokenKind::Shr => "'>>'".to_string(),

        // Assignment
        TokenKind::Eq => "'='".to_string(),
        TokenKind::PlusEq => "'+='".to_string(),
        TokenKind::MinusEq => "'-='".to_string(),
        TokenKind::StarEq => "'*='".to_string(),
        TokenKind::SlashEq => "'/='".to_string(),
        TokenKind::PercentEq => "'%='".to_string(),
        TokenKind::AmpEq => "'&='".to_string(),
        TokenKind::PipeEq => "'|='".to_string(),
        TokenKind::CaretEq => "'^='".to_string(),
        TokenKind::ShlEq => "'<<='".to_string(),
        TokenKind::ShrEq => "'>>='".to_string(),

        // Punctuation
        TokenKind::LParen => "'('".to_string(),
        TokenKind::RParen => "')'".to_string(),
        TokenKind::LBracket => "'['".to_string(),
        TokenKind::RBracket => "']'".to_string(),
        TokenKind::LBrace => "'{'".to_string(),
        TokenKind::RBrace => "'}'".to_string(),
        TokenKind::Comma => "','".to_string(),
        TokenKind::Dot => "'.'".to_string(),
        TokenKind::DotDot => "'..".to_string(),
        TokenKind::DotDotDot => "'...'".to_string(),
        TokenKind::Colon => "':'".to_string(),
        TokenKind::ColonColon => "'::'".to_string(),
        TokenKind::Semi => "';'".to_string(),
        TokenKind::Arrow => "'->'".to_string(),
        TokenKind::FatArrow => "'=>'".to_string(),
        TokenKind::At => "'@'".to_string(),
        TokenKind::QuestionQuestion => "'??'".to_string(),
        TokenKind::QuestionDot => "'?.'".to_string(),
        TokenKind::Question => "'?'".to_string(),

        // JSX-like
        TokenKind::LtSlash => "'</'".to_string(),

        // Whitespace
        TokenKind::Newline(_) => "newline".to_string(),
        TokenKind::Comment => "comment".to_string(),
        TokenKind::HashComment => "comment".to_string(),
        TokenKind::Indent => "indentation".to_string(),
        TokenKind::Dedent => "dedentation".to_string(),
        TokenKind::Eof => "end of file".to_string(),
    }
}

/// Convert a Token to user-friendly string representation
pub fn token_to_user_string(token: &crate::lexer::Token) -> String {
    token_kind_to_user_string(&token.kind)
}
