

# Motif

**Motif** is a C++ command-line tool for **audio thumbnailing**.

Given an input audio file and an output folder, Motif analyzes the musical structure of the recording and automatically extracts a representative segment. The generated result can serve as a short preview, a structural thumbnail, or a motif-like summary of the piece.

## Features

- Command-line interface for direct audio processing
- Automatic extraction of representative musical segments
- Repetition-based audio thumbnailing pipeline
- End-to-end workflow from input audio to exported result
- Multilingual help output
- Runtime log inspection
- Version command for CLI release tracking

## What Motif Does

Motif takes an audio file as input, processes it through a music structure analysis pipeline, and exports a shortened representative result.

Internally, the system is based on repetition-oriented audio thumbnailing ideas. In practical terms, the pipeline includes:

- audio preparation and workspace staging
- chroma-based feature extraction
- self-similarity analysis
- candidate segment evaluation
- thumbnail selection
- cutting and fade processing
- final motif-style export

## Command Line Usage

```bash
motif <audio.xxx> <output-folder>
motif <command>
```

### Examples

```bash
motif input.wav ./output
motif song.m4a ./result
motif help
motif zh
motif ja
motif log
motif --version
```

## Commands

### Help

```bash
motif help
motif -h
motif --help
```

Show the default help message in English.

### Chinese Help

```bash
motif zh
motif --zh
```

Show the help message in Chinese.

### Japanese Help

```bash
motif ja
motif --ja
```

Show the help message in Japanese.

### Version

```bash
motif -v
motif --version
motif -version
motif -ver
```

Print the current CLI version.

### Log

```bash
motif log
```

Print runtime logs.

## Basic Workflow

When two file paths are provided, Motif runs the audio thumbnailing workflow.

```bash
motif <audio.xxx> <output-folder>
```

The tool will:

1. initialize the runtime environment
2. clear the internal workspace
3. copy the input audio into the workspace
4. run the core audio thumbnailing algorithm
5. generate intermediate processed audio results
6. rename the final prepared output as a motif file
7. copy the generated file to the user-specified output folder

## Output Behavior

During processing, Motif internally produces several stage-specific files, including variants such as:

- raw cut result
- faded thumbnail result
- final exported motif result

In the current implementation, the final exported file is produced under the naming scheme:

```text
<title>_motif.m4a
```

This file is then copied into the output folder specified by the user.

## Current CLI Identity

Application name:

```text
motif
```

Current version:

```text
0.1.0
```

## Example Session

```bash
motif my_song.m4a ./exports
```

Possible processing flow:

- input audio is copied into the workspace
- thumbnail extraction is performed
- processed audio is generated
- final file such as `my_song_motif.m4a` is copied to `./exports`

## Implementation Notes

Motif is implemented in C++ and currently uses an internal modular structure. The CLI entry coordinates the following responsibilities:

- command parsing
- help and version display
- runtime initialization
- workspace management
- invocation of the core `Motif::audioThumbnailing(...)` logic
- export of the final output file

The codebase also integrates several utility and processing modules, including components related to:

- logging
- file management
- audio conversion and processing
- path handling
- common runtime initialization

## Design Orientation

Motif is intended as a practical command-line wrapper around a music structure analysis pipeline.

Rather than exposing every internal algorithmic detail directly through the CLI, the current design favors a simple usage model:

- provide an input audio file
- provide an output location
- let the system generate a representative motif automatically

This makes the tool suitable for experimentation, local batch scripting, and future extension into larger audio-processing workflows.

## Roadmap

Possible future directions include:

- richer CLI argument support
- configurable output format
- duration controls for extracted thumbnails
- optional preservation of intermediate files
- batch processing over folders
- explicit analysis/report output
- visualization of structural features and similarity matrices

## Build

This project is written in C++ and depends on your local build environment and linked modules.
You may need to configure compiler flags, include paths, and third-party dependencies according to your own setup.

## License

Apache License

## Acknowledgements

This project benefited from the educational resources developed by the International Audio Laboratories Erlangen, in particular the FMP (Fundamentals of Music Processing) notebooks by Meinard Müller and collaborators.

We would like to thank them for their comprehensive tutorial on audio thumbnailing, which combines theoretical explanations with practical implementations:

https://www.audiolabs-erlangen.de/resources/MIR/FMP/C4/C4S3_AudioThumbnailing.html

We also acknowledge the work "Towards Efficient Audio Thumbnailing", which provides important algorithmic insights.