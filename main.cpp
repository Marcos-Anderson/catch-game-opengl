#include <iostream>
#include <vector>
#include <random>
#include <sstream>
#include <glad/glad.h>   // GLAD deve vir sempre antes do GLFW (gerencia ponteiros de funções OpenGL)
#include <GLFW/glfw3.h>

// Definição das dimensões da janela do jogo em pixels
const int LARGURA = 800;
const int ALTURA = 600;

// Estrutura para representar formas retangulares (posição e tamanho)
struct Retangulo {
    float x, y, largura, altura;
};

// Estrutura para os objetos caindo do céu
struct Objeto {
    Retangulo corpo;  // Caixa de colisão e desenho
    float velocidade; // Velocidade de queda atualizada dinamicamente
    bool bonus;       // Se verdadeiro, vale mais pontos e tem cor diferente
};

// Variáveis globais para gerenciar o estado da janela e objetos gráficos do OpenGL
GLFWwindow* janela = nullptr;
GLuint shaderProgram = 0; // Identificador do programa de Shader compilado
GLuint VAO = 0, VBO = 0;   // Identificadores dos buffers de vértices

// Inicialização das entidades do jogo
Retangulo jogador{350.0f, 540.0f, 100.0f, 20.0f}; // Jogador começa embaixo centralizado
std::vector<Objeto> objetos;                     // Lista de objetos ativos caindo

// Variáveis de controle de estado do jogo
int pontos = 0;
int vidas = 3;
bool jogoAcabou = false;
float tempoUltimoObjeto = 0.0f; // Marca de tempo da última geração de objeto
float intervaloObjeto = 1.3f;   // Tempo de espera (em segundos) entre novos objetos
float velocidadeJogador = 450.0f;

// Motor de geração de números pseudo-aleatórios usando o algoritmo Mersenne Twister
std::mt19937 gerador(std::random_device{}());

// Função auxiliar para sortear um valor float dentro de um intervalo [min, max]
float sortearFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gerador);
}

// Função auxiliar com 10% de chance de retornar verdadeiro (para criar objetos bônus)
bool sortearBonus() {
    std::uniform_int_distribution<int> dist(1, 10);
    return dist(gerador) == 1; // Retorna true se cair o número 1
}

// Detecção de colisão usando AABB (Axis-Aligned Bounding Box)
// Verifica se há sobreposição nos eixos X e Y simultaneamente
bool colisaoAABB(const Retangulo& a, const Retangulo& b) {
    return a.x < b.x + b.largura &&
           a.x + a.largura > b.x &&
           a.y < b.y + b.altura &&
           a.y + a.altura > b.y;
}

// Atualiza o texto da barra de título da janela conforme o estado do jogo
void atualizarTitulo() {
    std::stringstream ss;
    if (jogoAcabou) {
        ss << "Fim de jogo! Pontos: " << pontos << " | Pressione R para reiniciar ou ESC para sair";
    } else {
        ss << "Catch Game 2D - Pontos: " << pontos << " | Vidas: " << vidas;
    }
    glfwSetWindowTitle(janela, ss.str().c_str());
}

// Instancia um novo objeto no topo da tela em uma posição X aleatória
void criarObjeto() {
    Objeto obj;
    obj.corpo.largura = 32.0f;
    obj.corpo.altura = 32.0f;
    // Sorteia a posição X garantindo que o objeto não nasça cortado para fora da tela
    obj.corpo.x = sortearFloat(0.0f, LARGURA - obj.corpo.largura);
    obj.corpo.y = -obj.corpo.altura; // Começa um pouco acima do topo visível
    obj.bonus = sortearBonus();

    // Aumenta a velocidade progressivamente com base na pontuação atual
    float aumento = pontos * 3.0f;
    obj.velocidade = obj.bonus ? 170.0f + aumento : 150.0f + aumento;

    objetos.push_back(obj);
}

// Reseta todas as variáveis para o estado inicial para reiniciar a partida
void reiniciarJogo() {
    pontos = 0;
    vidas = 3;
    jogoAcabou = false;
    jogador.x = 350.0f;
    jogador.y = 540.0f;
    objetos.clear();
    tempoUltimoObjeto = 0.0f;
    intervaloObjeto = 0.9f;
    atualizarTitulo();
}

// Captura e processa inputs de teclado do usuário
void processarEntrada(float deltaTempo) {
    // Tecla ESC fecha o jogo a qualquer momento
    if (glfwGetKey(janela, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(janela, true);
    }

    // Se o jogo acabou, as únicas ações permitidas são resetar ou fechar
    if (jogoAcabou) {
        if (glfwGetKey(janela, GLFW_KEY_R) == GLFW_PRESS) {
            reiniciarJogo();
        }
        return;
    }

    // Movimentação para a esquerda (Setas ou A) multiplicada pelo deltaTempo para ficar independente da taxa de FPS
    if (glfwGetKey(janela, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(janela, GLFW_KEY_A) == GLFW_PRESS) {
        jogador.x -= velocidadeJogador * deltaTempo;
    }

    // Movimentação para a direita (Setas ou D)
    if (glfwGetKey(janela, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(janela, GLFW_KEY_D) == GLFW_PRESS) {
        jogador.x += velocidadeJogador * deltaTempo;
    }

    // Garante que o jogador não saia das bordas laterais da janela (Clamping)
    if (jogador.x < 0.0f) jogador.x = 0.0f;
    if (jogador.x + jogador.largura > LARGURA) jogador.x = LARGURA - jogador.largura;
}

// Atualiza toda a física e regras de negócio do jogo a cada frame
void atualizarJogo(float deltaTempo, float tempoAtual) {
    if (jogoAcabou) return;

    // Lógica de Spawn temporizado de novos objetos caindo
    if (tempoAtual - tempoUltimoObjeto >= intervaloObjeto) {
        criarObjeto();
        tempoUltimoObjeto = tempoAtual;

        // Dificulta o jogo reduzindo o tempo de intervalo entre os objetos (mínimo de 0.35s)
        if (intervaloObjeto > 0.35f) {
            intervaloObjeto -= 0.015f;
        }
    }

    // Varre o vetor de trás para frente para evitar bugs de índice ao remover objetos ('erase')
    for (int i = (int)objetos.size() - 1; i >= 0; i--) {
        // Aplica o movimento de queda baseado na velocidade individual e tempo decorrido
        objetos[i].corpo.y += objetos[i].velocidade * deltaTempo;

        // Caso ocorra colisão entre o jogador e o objeto caindo
        if (colisaoAABB(jogador, objetos[i].corpo)) {
            if (objetos[i].bonus) {
                pontos += 3; // Bônus vale 3 pontos
            } else {
                pontos += 1; // Comum vale 1 ponto
            }
            objetos.erase(objetos.begin() + i); // Remove o objeto coletado
            atualizarTitulo();
            continue;
        }

        // Caso o objeto ultrapasse o limite inferior da tela (jogador não pegou)
        if (objetos[i].corpo.y > ALTURA) {
            vidas--; // Perde uma vida
            objetos.erase(objetos.begin() + i);
            atualizarTitulo();

            // Verifica a condição de Game Over
            if (vidas <= 0) {
                vidas = 0;
                jogoAcabou = true;
                atualizarTitulo();
                return;
            }
        }
    }
}

// Função utilitária para compilar fontes de Vertex e Fragment Shaders em runtime
GLuint compilarShader(GLenum tipo, const char* codigo) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &codigo, nullptr);
    glCompileShader(shader);

    // Validação de erros de compilação
    int sucesso;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "Erro ao compilar shader:\n" << infoLog << std::endl;
    }
    return shader;
}

// Cria o programa de Shaders do pipeline moderno do OpenGL (versão 330 core)
void criarShader() {
    // Vertex Shader: Responsável por calcular as posições dos vértices em coordenadas normalizadas (Clip Space: -1 a 1)
    const char* vertexShaderCodigo = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos; // Vértice unitário de entrada
        uniform vec2 uPosicao;   // Posição x, y real na tela
        uniform vec2 uTamanho;   // Largura e altura real na tela
        uniform vec2 uResolucao; // Resolução atual (800x600)

        void main() {
            // Mapeia e escala o quadrado unitário básico para as coordenadas de pixel informadas pelas Uniforms
            vec2 posicao = aPos * uTamanho + uPosicao;
            // Transforma o sistema de coordenadas de tela (0 a Largura/Altura) para o padrão OpenGL (-1.0 a 1.0)
            vec2 clip = (posicao / uResolucao) * 2.0 - 1.0;
            // Inverte o eixo Y porque no OpenGL o Y cresce para cima, mas no nosso jogo cresce para baixo
            clip.y = -clip.y;
            gl_Position = vec4(clip, 0.0, 1.0);
        }
    )";

    // Fragment Shader: Define a cor final de cada pixel gerado na tela
    const char* fragmentShaderCodigo = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 uCor; // Cor RGB enviada via CPU

        void main() {
            FragColor = vec4(uCor, 1.0); // Define a cor final do pixel (Alpha fixo em 1.0)
        }
    )";

    // Compilação individual dos shaders
    GLuint vertexShader = compilarShader(GL_VERTEX_SHADER, vertexShaderCodigo);
    GLuint fragmentShader = compilarShader(GL_FRAGMENT_SHADER, fragmentShaderCodigo);

    // Linkagem dos shaders compilados em um único programa executável pela GPU
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Validação de erros de linkagem do programa
    int sucesso;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &sucesso);
    if (!sucesso) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "Erro ao linkar shader:\n" << infoLog << std::endl;
    }

    // Uma vez linkados ao programa, os objetos shader individuais podem ser liberados
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

// Inicializa a geometria de um quadrado unitário (0.0 a 1.0) usando dois triângulos
void prepararRetangulo() {
    float vertices[] = {
        // Primeiro Triângulo
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        // Segundo Triângulo
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    // Cria e vincula o Vertex Array Object e Vertex Buffer Object
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Envia os vértices para a memória dedicada da GPU
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Configura o layout dos dados para o Vertex Shader (Location 0, composto por 2 floats por vértice)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// Envia os parâmetros de transformação e cor específicos do retângulo para as Uniforms do Shader e desenha
void desenharRetangulo(const Retangulo& r, float vermelho, float verde, float azul) {
    glUseProgram(shaderProgram);

    // Atualiza as propriedades Uniform na GPU com as características do retângulo atual
    glUniform2f(glGetUniformLocation(shaderProgram, "uPosicao"), r.x, r.y);
    glUniform2f(glGetUniformLocation(shaderProgram, "uTamanho"), r.largura, r.altura);
    glUniform2f(glGetUniformLocation(shaderProgram, "uResolucao"), (float)LARGURA, (float)ALTURA);
    glUniform3f(glGetUniformLocation(shaderProgram, "uCor"), vermelho, verde, azul);

    // Desenha o quadrado usando os 6 vértices mapeados no VAO
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// Desenha visualmente a barra de pontuação na tela usando pequenos blocos verdes
void desenharBarraDePontos() {
    int blocos = pontos;
    if (blocos > 20) blocos = 20; // Limita o tamanho visual na tela para não estourar o layout

    for (int i = 0; i < blocos; i++) {
        Retangulo bloco{20.0f + i * 18.0f, 20.0f, 14.0f, 14.0f};
        desenharRetangulo(bloco, 0.1f, 0.8f, 0.2f); // Cor verde
    }
}

// Desenha a quantidade de vidas restantes em forma de blocos vermelhos
void desenharVidas() {
    for (int i = 0; i < vidas; i++) {
        Retangulo vida{20.0f + i * 30.0f, 50.0f, 22.0f, 22.0f};
        desenharRetangulo(vida, 0.9f, 0.1f, 0.1f); // Cor vermelha
    }
}

// ============================================================
// SISTEMA DE FONTE PIXEL (5 colunas x 7 linhas por glifo)
// Cada glifo é um array de 7 strings de 5 chars: 'X' = pixel ativo, ' ' = vazio.
// Não requer nenhuma biblioteca externa de fonte.
// ============================================================

// Retorna o glifo (bitmap 5x7) correspondente ao caractere solicitado.
// Maiúsculas do alfabeto necessárias para "GAME OVER / PRESS R TO RESTART / ESC TO EXIT".
const char** obterGlifo(char c) {
    static const char* GLIFO_A[7] = {" XXX ", "X   X", "X   X", "XXXXX", "X   X", "X   X", "X   X"};
    static const char* GLIFO_C[7] = {" XXX ", "X   X", "X    ", "X    ", "X    ", "X   X", " XXX "};
    static const char* GLIFO_E[7] = {"XXXXX", "X    ", "X    ", "XXXX ", "X    ", "X    ", "XXXXX"};
    static const char* GLIFO_G[7] = {" XXX ", "X    ", "X    ", "X  XX", "X   X", "X   X", " XXX "};
    static const char* GLIFO_I[7] = {" XXX ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", " XXX "};
    static const char* GLIFO_M[7] = {"X   X", "XX XX", "X X X", "X   X", "X   X", "X   X", "X   X"};
    static const char* GLIFO_O[7] = {" XXX ", "X   X", "X   X", "X   X", "X   X", "X   X", " XXX "};
    static const char* GLIFO_P[7] = {"XXXX ", "X   X", "X   X", "XXXX ", "X    ", "X    ", "X    "};
    static const char* GLIFO_R[7] = {"XXXX ", "X   X", "X   X", "XXXX ", "X X  ", "X  X ", "X   X"};
    static const char* GLIFO_S[7] = {" XXXX", "X    ", "X    ", " XXX ", "    X", "    X", "XXXX "};
    static const char* GLIFO_T[7] = {"XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  "};
    static const char* GLIFO_V[7] = {"X   X", "X   X", "X   X", "X   X", " X X ", " X X ", "  X  "};
    static const char* GLIFO_X[7] = {"X   X", "X   X", " X X ", "  X  ", " X X ", "X   X", "X   X"};
    static const char* GLIFO_SP[7] = {"     ", "     ", "     ", "     ", "     ", "     ", "     "};

    switch (c) {
        case 'A': return GLIFO_A;
        case 'C': return GLIFO_C;
        case 'E': return GLIFO_E;
        case 'G': return GLIFO_G;
        case 'I': return GLIFO_I;
        case 'M': return GLIFO_M;
        case 'O': return GLIFO_O;
        case 'P': return GLIFO_P;
        case 'R': return GLIFO_R;
        case 'S': return GLIFO_S;
        case 'T': return GLIFO_T;
        case 'V': return GLIFO_V;
        case 'X': return GLIFO_X;
        default:  return GLIFO_SP; // Espaço ou caractere não mapeado
    }
}

// Calcula a largura em pixels de tela que uma string ocupa com determinado pixelSize.
// Fórmula: N chars * (5 colunas + 1 de gap) * pixelSize — 1 gap no último char.
float larguraTexto(const char* texto, float pixelSize) {
    int n = 0;
    while (texto[n] != '\0') n++;
    if (n == 0) return 0.0f;
    return (float)(n * 6 - 1) * pixelSize;
}

// Desenha um único caractere pixel-a-pixel a partir de (x, y) usando retângulos.
// pixelSize: tamanho em pixels de tela de cada "pixel" do glifo.
void desenharCaractere(char c, float x, float y, float pixelSize,
                       float r, float g, float b) {
    const char** glifo = obterGlifo(c);
    for (int linha = 0; linha < 7; linha++) {
        for (int col = 0; col < 5; col++) {
            if (glifo[linha][col] == 'X') {
                Retangulo pixel{
                    x + col * pixelSize,
                    y + linha * pixelSize,
                    pixelSize - 0.5f, // leve gap entre pixels para visual retrô
                    pixelSize - 0.5f
                };
                desenharRetangulo(pixel, r, g, b);
            }
        }
    }
}

// Desenha uma string inteira horizontalmente a partir de (x, y).
void desenharTexto(const char* texto, float x, float y, float pixelSize,
                   float r, float g, float b) {
    float cursorX = x;
    for (int i = 0; texto[i] != '\0'; i++) {
        desenharCaractere(texto[i], cursorX, y, pixelSize, r, g, b);
        cursorX += (5 + 1) * pixelSize; // avança 5 colunas + 1 pixel de separação
    }
}

// Centraliza horizontalmente e desenha o texto na posição Y indicada.
void desenharTextoCentrado(const char* texto, float y, float pixelSize,
                           float r, float g, float b) {
    float x = ((float)LARGURA - larguraTexto(texto, pixelSize)) / 2.0f;
    desenharTexto(texto, x, y, pixelSize, r, g, b);
}

// ============================================================
// Tela de Fim de Jogo — substitui o placeholder por texto pixel
// real exibindo "GAME OVER" e as instruções de reinício/saída,
// lidas diretamente das teclas mapeadas em processarEntrada().
//   • Reiniciar → tecla R  (GLFW_KEY_R)
//   • Sair      → tecla ESC (GLFW_KEY_ESCAPE)
// ============================================================
void desenharFimDeJogo() {
    // --- Painel de fundo ---
    // Fundo escuro cobrindo a região central da tela
    Retangulo fundo{90.0f, 145.0f, 620.0f, 295.0f};
    desenharRetangulo(fundo, 0.04f, 0.04f, 0.07f);

    // Borda vermelha escura ao redor do painel (4 tiras finas)
    desenharRetangulo({90.0f,  145.0f, 620.0f,   3.0f}, 0.55f, 0.08f, 0.08f); // topo
    desenharRetangulo({90.0f,  437.0f, 620.0f,   3.0f}, 0.55f, 0.08f, 0.08f); // base
    desenharRetangulo({90.0f,  145.0f,   3.0f, 295.0f}, 0.55f, 0.08f, 0.08f); // esq
    desenharRetangulo({707.0f, 145.0f,   3.0f, 295.0f}, 0.55f, 0.08f, 0.08f); // dir

    // --- "GAME OVER" — pixelSize 6 → glifo 30×42 px por char ---
    // Sombra (deslocada 2px para baixo/direita, cor escura)
    desenharTextoCentrado("GAME OVER", 177.0f, 6.0f, 0.35f, 0.0f, 0.0f);
    // Texto principal vermelho vibrante
    desenharTextoCentrado("GAME OVER", 175.0f, 6.0f, 0.95f, 0.18f, 0.18f);

    // Linha divisória cinza entre título e instruções
    Retangulo divisor{160.0f, 240.0f, 480.0f, 2.0f};
    desenharRetangulo(divisor, 0.30f, 0.30f, 0.30f);

    // --- Instrução de reinício — tecla R (mapeada em processarEntrada) ---
    // pixelSize 3 → glifo 15×21 px por char
    // Rótulo da tecla em amarelo, restante em cinza claro
    desenharTexto("R",
        ((float)LARGURA - larguraTexto("PRESS R TO RESTART", 3.0f)) / 2.0f
            + larguraTexto("PRESS ", 3.0f),
        265.0f, 3.0f, 0.98f, 0.82f, 0.15f); // "R" em amarelo

    // String completa em cinza (a letra R amarela será sobrescrita acima)
    desenharTextoCentrado("PRESS R TO RESTART", 265.0f, 3.0f, 0.80f, 0.80f, 0.80f);

    // Redesenha só o "R" em amarelo por cima para destacá-lo
    {
        float baseX = ((float)LARGURA - larguraTexto("PRESS R TO RESTART", 3.0f)) / 2.0f;
        float rX    = baseX + larguraTexto("PRESS ", 3.0f);
        desenharCaractere('R', rX, 265.0f, 3.0f, 0.98f, 0.82f, 0.15f);
    }

    // --- Instrução de saída — tecla ESC (mapeada em processarEntrada) ---
    desenharTextoCentrado("ESC TO EXIT", 310.0f, 3.0f, 0.55f, 0.55f, 0.55f);
    // "ESC" em tom levemente avermelhado para indicar perigo/saída
    {
        float baseX = ((float)LARGURA - larguraTexto("ESC TO EXIT", 3.0f)) / 2.0f;
        desenharTexto("ESC", baseX, 310.0f, 3.0f, 0.90f, 0.40f, 0.40f);
    }

    // Pontuação final — barra de blocos dourados centralizada na base do painel
    // (o valor numérico exato também aparece na barra de título via atualizarTitulo)
    int blocosPontos = pontos > 24 ? 24 : pontos;
    float scoreBaseX = (float)LARGURA / 2.0f - blocosPontos * 9.0f / 2.0f;
    for (int i = 0; i < blocosPontos; i++) {
        Retangulo bloco{scoreBaseX + i * 9.0f, 395.0f, 7.0f, 7.0f};
        desenharRetangulo(bloco, 0.98f, 0.75f, 0.1f);
    }
}

// Concentra todas as diretivas de desenho do frame
void renderizar() {
    // Define a cor de fundo (Azul escuro/cinza) e limpa os buffers de cores anteriores
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Desenha o jogador (Retângulo azul)
    desenharRetangulo(jogador, 0.1f, 0.35f, 0.9f);

    // Itera e desenha cada um dos objetos caindo ativos
    for (const auto& obj : objetos) {
        if (obj.bonus) {
            desenharRetangulo(obj.corpo, 1.0f, 0.85f, 0.1f); // Bônus = Amarelo/Dourado
        } else {
            desenharRetangulo(obj.corpo, 0.9f, 0.1f, 0.1f);   // Comum = Vermelho
        }
    }

    // Renderiza os elementos de interface (HUD)
    desenharBarraDePontos();
    desenharVidas();

    // Se o jogo acabou, renderiza o painel de fim de jogo por cima
    if (jogoAcabou) {
        desenharFimDeJogo();
    }
}

// Função de callback acionada sempre que a janela for redimensionada pelo usuário
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Ajusta a Viewport para manter os desenhos corretos com base no novo tamanho
    glViewport(0, 0, width, height);
}

// Função principal / Ponto de entrada do sistema
int main() {
    // Inicialização da biblioteca GLFW
    if (!glfwInit()) {
        std::cout << "Erro ao iniciar GLFW.\n";
        return -1;
    }

    // Configuração dos contextos mínimos do OpenGL (Versão 3.3 Core Profile)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criação da janela nativa do sistema operacional
    janela = glfwCreateWindow(LARGURA, ALTURA, "Catch Game 2D", nullptr, nullptr);
    if (janela == nullptr) {
        std::cout << "Erro ao criar janela GLFW.\n";
        glfwTerminate();
        return -1;
    }

    // Torna o contexto da janela que criamos atual na thread principal
    glfwMakeContextCurrent(janela);
    glfwSetFramebufferSizeCallback(janela, framebuffer_size_callback);

    // Inicialização do GLAD para carregar todas as funções modernas do OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Erro ao iniciar GLAD.\n";
        glfwTerminate();
        return -1;
    }

    // Configurações iniciais de renderização e estado do jogo
    criarShader();
    prepararRetangulo();
    atualizarTitulo();

    // Controle de tempo para cálculo do Delta Time (Garante velocidade constante independente do PC)
    float ultimoTempo = (float)glfwGetTime();

    // Loop principal da aplicação (Game Loop)
    while (!glfwWindowShouldClose(janela)) {
        float tempoAtual = (float)glfwGetTime();
        float deltaTempo = tempoAtual - ultimoTempo;
        ultimoTempo = tempoAtual;

        // Três estágios principais de cada iteração do loop
        processarEntrada(deltaTempo);
        atualizarJogo(deltaTempo, tempoAtual);
        renderizar();

        // Inverte os buffers de vídeo (Double Buffering) para evitar screen tearing (piscadas na tela)
        glfwSwapBuffers(janela);
        // Processa eventos pendentes do sistema operacional (como cliques do mouse e teclado)
        glfwPollEvents();
    }

    // Desalocação e limpeza de buffers e programas na GPU após sair do loop principal
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    // Finaliza e encerra a execução do GLFW de forma segura
    glfwTerminate();
    return 0;
}
