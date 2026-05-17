# Catch Game 2D - OpenGL

Atividade prática de Computação Gráfica 2026.1.

## Descrição

Jogo 2D feito em C++ com OpenGL, GLFW e GLAD.

O jogador controla uma barra horizontal na parte inferior da tela e precisa capturar os objetos que caem do topo. Cada objeto capturado aumenta a pontuação, enquanto objetos perdidos reduzem as vidas.

## Funcionalidades

- Movimento horizontal do jogador
- Objetos caindo continuamente
- Colisão AABB
- Sistema de pontuação
- Sistema de vidas
- Fim de jogo ao perder todas as vidas
- Reinício do jogo com a tecla R

## Controles

- Seta esquerda ou A: mover para esquerda
- Seta direita ou D: mover para direita
- R: reiniciar jogo
- ESC: sair

## Como compilar no Windows

No terminal, dentro da pasta do projeto, execute:

```bash
g++ main.cpp src/glad.c -o jogo.exe -Iinclude -Llib -lglfw3dll -lopengl32 -lgdi32
