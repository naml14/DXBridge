#![cfg(windows)]

use std::collections::{HashMap, HashSet};
use std::error::Error;

pub type AnyResult<T> = Result<T, Box<dyn Error>>;

#[derive(Debug, Default)]
pub struct CommandLine {
    values: HashMap<String, String>,
    flags: HashSet<String>,
    pub positional: Vec<String>,
    pub help_requested: bool,
}

impl CommandLine {
    pub fn from_env() -> AnyResult<Self> {
        let args = std::env::args().skip(1).collect::<Vec<_>>();
        Self::parse(args)
    }

    pub fn parse(args: Vec<String>) -> AnyResult<Self> {
        let mut cli = Self::default();
        let mut index = 0usize;

        while index < args.len() {
            let arg = &args[index];

            if arg == "--help" || arg == "-h" {
                cli.help_requested = true;
                index += 1;
                continue;
            }

            if let Some(rest) = arg.strip_prefix("--") {
                if let Some((key, value)) = rest.split_once('=') {
                    cli.values.insert(key.to_string(), value.to_string());
                    index += 1;
                    continue;
                }

                let key = rest.to_string();
                if index + 1 < args.len() && !args[index + 1].starts_with("--") {
                    cli.values.insert(key, args[index + 1].clone());
                    index += 2;
                } else {
                    cli.flags.insert(key);
                    index += 1;
                }
                continue;
            }

            cli.positional.push(arg.clone());
            index += 1;
        }

        Ok(cli)
    }

    pub fn has_flag(&self, name: &str) -> bool {
        self.flags.contains(name.trim_start_matches('-'))
    }

    pub fn get_string(&self, name: &str, default: &str) -> String {
        self.values
            .get(name.trim_start_matches('-'))
            .cloned()
            .unwrap_or_else(|| default.to_string())
    }

    pub fn get_optional_string(&self, name: &str) -> Option<String> {
        self.values.get(name.trim_start_matches('-')).cloned()
    }

    pub fn get_i32(&self, name: &str, default: i32) -> AnyResult<i32> {
        self.values
            .get(name.trim_start_matches('-'))
            .map(|value| {
                value
                    .parse::<i32>()
                    .map_err(|err| format!("invalid value for {}: {}", name, err).into())
            })
            .unwrap_or(Ok(default))
    }

    pub fn get_u32(&self, name: &str, default: u32) -> AnyResult<u32> {
        self.values
            .get(name.trim_start_matches('-'))
            .map(|value| {
                value
                    .parse::<u32>()
                    .map_err(|err| format!("invalid value for {}: {}", name, err).into())
            })
            .unwrap_or(Ok(default))
    }

    pub fn get_f32(&self, name: &str, default: f32) -> AnyResult<f32> {
        self.values
            .get(name.trim_start_matches('-'))
            .map(|value| {
                value
                    .parse::<f32>()
                    .map_err(|err| format!("invalid value for {}: {}", name, err).into())
            })
            .unwrap_or(Ok(default))
    }

    pub fn get_f64(&self, name: &str, default: f64) -> AnyResult<f64> {
        self.values
            .get(name.trim_start_matches('-'))
            .map(|value| {
                value
                    .parse::<f64>()
                    .map_err(|err| format!("invalid value for {}: {}", name, err).into())
            })
            .unwrap_or(Ok(default))
    }
}
