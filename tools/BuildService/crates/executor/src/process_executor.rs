//! Process execution and log capture

use anyhow::{anyhow, Context, Result};
use std::io::{BufRead, BufReader, Write};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::thread;

/// Streaming configuration for process execution
#[derive(Debug, Clone)]
pub struct ProcessExecutorConfig {
    pub stream_output: bool,
}

impl Default for ProcessExecutorConfig {
    fn default() -> Self {
        Self {
            stream_output: false,
        }
    }
}

/// Process executor for running UAT commands
#[derive(Debug, Clone)]
pub struct ProcessExecutor {
    config: ProcessExecutorConfig,
}

impl ProcessExecutor {
    pub fn new() -> Self {
        Self {
            config: ProcessExecutorConfig::default(),
        }
    }

    pub fn with_config(config: ProcessExecutorConfig) -> Self {
        Self { config }
    }

    pub fn with_stream_output(stream_output: bool) -> Self {
        Self {
            config: ProcessExecutorConfig { stream_output },
        }
    }

    /// Execute a command and capture output to log file
    pub fn execute(&self, command: &str, log_path: &PathBuf) -> Result<bool> {
        tracing::info!("Executing command: {}", command);
        tracing::info!("Log file: {}", log_path.display());

        // Create log directory if it doesn't exist
        if let Some(parent) = log_path.parent() {
            std::fs::create_dir_all(parent)?;
        }

        // Parse command into program and args
        let parts: Vec<&str> = command.split_whitespace().collect();
        if parts.is_empty() {
            return Err(anyhow::anyhow!("Empty command"));
        }

        let program = parts[0];
        let args = &parts[1..];

        // Execute command
        let mut child = Command::new(program)
            .args(args)
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .context("Failed to spawn process")?;

        let log_file = Arc::new(Mutex::new(std::fs::File::create(log_path)?));
        let mut handles = Vec::new();

        if let Some(stdout) = child.stdout.take() {
            handles.push(Self::spawn_stream_thread(
                stdout,
                log_file.clone(),
                self.config.stream_output,
                "[UE][STDOUT]",
            ));
        }

        if let Some(stderr) = child.stderr.take() {
            handles.push(Self::spawn_stream_thread(
                stderr,
                log_file.clone(),
                self.config.stream_output,
                "[UE][STDERR]",
            ));
        }

        let status = child.wait()?;

        for handle in handles {
            handle
                .join()
                .map_err(|e| anyhow!("Stream thread panicked: {:?}", e))??;
        }

        let success = status.success();

        if success {
            tracing::info!("Build completed successfully");
        } else {
            tracing::error!("Build failed with exit code: {:?}", status.code());
        }

        Ok(success)
    }

    /// Execute command and stream output to console
    pub fn execute_with_output(&self, command: &str) -> Result<bool> {
        tracing::info!("Executing command: {}", command);

        let parts: Vec<&str> = command.split_whitespace().collect();
        if parts.is_empty() {
            return Err(anyhow::anyhow!("Empty command"));
        }

        let program = parts[0];
        let args = &parts[1..];

        let status = Command::new(program)
            .args(args)
            .status()
            .context("Failed to execute command")?;

        Ok(status.success())
    }

    fn spawn_stream_thread<R: std::io::Read + Send + 'static>(
        reader: R,
        log_file: Arc<Mutex<std::fs::File>>,
        stream_output: bool,
        prefix: &'static str,
    ) -> thread::JoinHandle<Result<()>> {
        thread::spawn(move || -> Result<()> {
            let mut buf_reader = BufReader::new(reader);
            let mut line = String::new();
            loop {
                line.clear();
                let bytes = buf_reader.read_line(&mut line)?;
                if bytes == 0 {
                    break;
                }

                {
                    let mut file = log_file
                        .lock()
                        .map_err(|_| anyhow!("Failed to lock log file for writing"))?;
                    file.write_all(line.as_bytes())?;
                }

                if stream_output {
                    let trimmed = line.trim_end_matches(&['\r', '\n'][..]);
                    if !trimmed.is_empty() {
                        println!("  {} {}", prefix, trimmed);
                    }
                }
            }

            Ok(())
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_execute_simple_command() {
        let executor = ProcessExecutor::new();
        let temp_dir = env::temp_dir();
        let log_path = temp_dir.join("test_executor.log");

        // Execute a simple command that should succeed on Windows
        #[cfg(target_os = "windows")]
        let result = executor.execute("cmd /c echo test", &log_path);

        #[cfg(not(target_os = "windows"))]
        let result = executor.execute("echo test", &log_path);

        assert!(result.is_ok());

        // Cleanup
        let _ = std::fs::remove_file(log_path);
    }
}
