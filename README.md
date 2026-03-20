![license](https://img.shields.io/badge/License-mit-green)  ![badge](https://img.shields.io/badge/Lang-C-blue)
# 🖥️ *Fastproc-C* 
<!-- ![CLI Demo](https://media4.giphy.com/media/v1.Y2lkPTc5MGI3NjExNHB4OWQwd2h1eXF2M2RhaWdtdm12NmkyY2I3YjdrdHFtMDMxNXZoZCZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/jHZeHPMkhkCbgRcmfx/giphy.gif) -->
# fastproc++

## Índice

### Português

* [Visão Geral](#visão-geral)
* [Funcionalidades](#funcionalidades)
* [Controles](#controles)
* [Instalação](#instalação)
* [Como Funciona (Internamente)](#como-funciona-internamente)
* [Tecnologias Utilizadas](#tecnologias-utilizadas)

### English

* [Overview](#overview)
* [Features](#features)
* [Controls](#controls-1)
* [Installation](#installation)
* [How It Works (Internals)](#how-it-works-internals)
* [Technologies Used](#technologies-used)

---

# 🇧🇷 Português

## Visão Geral

**fastproc++** é um gerenciador de processos em modo terminal desenvolvido em C, utilizando ncurses. O projeto foi construído com foco em explorar conceitos de sistemas Linux, leitura direta do `/proc`, concorrência e manipulação eficiente de dados em tempo real.

---

## Funcionalidades

* Listagem de processos em tempo real
* Exibição de:

  * PID
  * Nome do processo
  * Uso de CPU
  * Uso de memória (RAM)
  * Número de threads
* Ordenação por:

  * RAM
  * CPU
  * PID
  * Threads
* Filtro por nome de processo
* Atualização contínua dos dados
* Navegação interativa via teclado
* Encerramento de processos (SIGTERM)

---

## Controles

* `W` / `↑` → mover seleção para cima
* `S` / `↓` → mover seleção para baixo
* `K` → encerrar processo selecionado
* `1` → ordenar por RAM
* `2` → ordenar por CPU
* `3` → ordenar por PID
* `4` → ordenar por Threads
* `Enter` → iniciar/parar busca por nome
* `Q` → sair

---

## Instalação

### 1. Compilar

```bash
make
```

---

### 2. Executar

```bash
./fastproc
```

---

### 3. Instalar no sistema

Instala o binário em `/usr/local/bin` e adiciona o atalho no menu:

```bash
sudo make install
```

---

### 4. Remover

```bash
sudo make remove
```

---

### 5. Limpar build

```bash
make clean
```

---

## Como Funciona (Internamente)

### 1. Coleta de dados (thread separada)

Uma thread dedicada:

* Percorre `/proc`
* Identifica processos
* Lê `/proc/[pid]/stat`
* Extrai CPU, memória e threads
* Calcula uso de CPU com base na diferença de ticks

Utiliza:

* HashMap para armazenar CPU anterior por PID
* Diferença entre leituras de `/proc/stat`

---

### 2. Interface (thread principal)

Responsável por:

* Renderização com ncurses
* Entrada do usuário
* Controle de ordenação e filtro

Para evitar inconsistência:

* Uso de mutex
* Deep copy dos dados antes de renderizar

---

### 3. Filtro e Ordenação

* Filtro por prefixo (`strncmp`)
* Ordenação manual com base no critério selecionado

---

## Tecnologias Utilizadas

* C
* pthread
* ncurses
* `/proc` (Linux)

---

# 🇺🇸 English

## Overview

**fastproc++** is a terminal-based process manager written in C using ncurses. It focuses on Linux internals, `/proc` parsing, concurrency, and real-time updates.

---

## Features

* Real-time process listing
* Displays:

  * PID
  * Process name
  * CPU usage
  * Memory usage
  * Thread count
* Sorting by:

  * RAM
  * CPU
  * PID
  * Threads
* Name filtering
* Continuous updates
* Interactive navigation
* Process termination (SIGTERM)

---

## Controls

* `W` / `↑` → move up
* `S` / `↓` → move down
* `K` → kill process
* `1` → sort by RAM
* `2` → sort by CPU
* `3` → sort by PID
* `4` → sort by Threads
* `Enter` → toggle search
* `Q` → quit

---

## Installation

### 1. Build

```bash
make
```

---

### 2. Run

```bash
./fastproc
```

---

### 3. Install system-wide

Installs the binary to `/usr/local/bin` and adds a desktop entry:

```bash
sudo make install
```

---

### 4. Remove

```bash
sudo make remove
```

---

### 5. Clean

```bash
make clean
```

---

## How It Works (Internals)

### 1. Data collection (separate thread)

* Scans `/proc`
* Reads `/proc/[pid]/stat`
* Extracts CPU, memory, threads
* Computes CPU usage using tick differences

Uses:

* HashMap for previous CPU values
* `/proc/stat` comparison

---

### 2. Interface (main thread)

* Renders UI with ncurses
* Handles input
* Manages sorting and filtering

Ensures safety with:

* Mutex
* Deep copy before rendering

---

### 3. Filtering and Sorting

* Prefix-based filtering (`strncmp`)
* Manual sorting

---

## Technologies Used

* C
* pthread
* ncurses
* `/proc` (Linux)

