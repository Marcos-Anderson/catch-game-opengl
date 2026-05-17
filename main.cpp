#include <iostream>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

const int LARGURA = 800;
const int ALTURA = 600;

struct Retangulo {
    float x, y, largura, altura;
};

struct Objeto {
    Retangulo corpo;
    float velocidade;
    bool bonus;
};

GLFWwindow* janela = nullptr;
GLuint shaderProgram = 0;
GLuint VAO = 0, VBO = 0;

Retangulo jogador{350.0f, 540.0f, 100.0f, 20.0f};
std::vector<Objeto> objetos;

int pontos = 0;
int vidas = 3;
bool jogoAcabou = false;
float tempoUltimoObjeto = 0.0f;
float intervaloObjeto = 0.9f;
float velocidadeJogador = 420.0f;

std::mt19937 gerador(std::random_device{}());

float sortearFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gerador);
}

bool sortearBonus() {
    std::uniform_int_distribution<int> dist(1, 10);
    return dist(gerador) == 1;
}

bool colisaoAABB(const Retangulo& a, const Retangulo& b) {
    return a.x < b.x + b.largura &&
           a.x + a.largura > b.x &&
           a.y < b.y + b.altura &&
           a.y + a.altura > b.y;
}

void atualizarTitulo() {
    std::stringstream ss;
    if (jogoAcabou) {
        ss << "Fim de jogo! Pontos: " << pontos << " | Pressione R para reiniciar ou ESC para sair";
    } else {
        ss << "Catch Game 2D - Pontos: " << pontos << " | Vidas: " << vidas;
    }
    glfwSetWindowTitle(janela, ss.str().c_str());
}

void criarObjeto() {
    Objeto obj;
    obj.corpo.largura = 32.0f;
    obj.corpo.altura = 32.0f;
    obj.corpo.x = sortearFloat(0.0f, LARGURA - obj.corpo.largura);
    obj.corpo.y = -obj.corpo.altura;
    obj.bonus = sortearBonus();

    float aumento = pontos * 8.0f;
    obj.velocidade = obj.bonus ? 150.0f + aumento : 180.0f + aumento;

    objetos.push_back(obj);
}

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

void processarEntrada(float deltaTempo) {
    if (glfwGetKey(janela, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(janela, true);
    }

    if (jogoAcabou) {
        if (glfwGetKey(janela, GLFW_KEY_R) == GLFW_PRESS) {
            reiniciarJogo();
        }
        return;
    }

    if (glfwGetKey(janela, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(janela, GLFW_KEY_A) == GLFW_PRESS) {
        jogador.x -= velocidadeJogador * deltaTempo;
    }

    if (glfwGetKey(janela, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(janela, GLFW_KEY_D) == GLFW_PRESS) {
        jogador.x += velocidadeJogador * deltaTempo;
    }

    if (jogador.x < 0.0f) jogador.x = 0.0f;
    if (jogador.x + jogador.largura > LARGURA) jogador.x = LARGURA - jogador.largura;
}

void atualizarJogo(float deltaTempo, float tempoAtual) {
    if (jogoAcabou) return;

    if (tempoAtual - tempoUltimoObjeto >= intervaloObjeto) {
        criarObjeto();
        tempoUltimoObjeto = tempoAtual;

        if (intervaloObjeto > 0.35f) {
            intervaloObjeto -= 0.015f;
        }
    }

    for (int i = (int)objetos.size() - 1; i >= 0; i--) {
        objetos[i].corpo.y += objetos[i].velocidade * deltaTempo;

        if (colisaoAABB(jogador, objetos[i].corpo)) {
            if (objetos[i].bonus) {
                pontos += 3;
            } else {
                pontos += 1;
            }
            objetos.erase(objetos.begin() + i);
            atualizarTitulo();
            continue;
        }

        if (objetos[i].corpo.y > ALTURA) {
            vidas--;
            objetos.erase(objetos.begin() + i);
            atualizarTitulo();

            if (vidas <= 0) {
                vidas = 0;
                jogoAcabou = true;
                atualizarTitulo();
                return;
            }
        }
    }
}

GLuint compilarShader(GLenum tipo, const char* codigo) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &codigo, nullptr);
    glCompileShader(shader);

    int sucesso;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "Erro ao compilar shader:\n" << infoLog << std::endl;
    }
    return shader;
}

void criarShader() {
    const char* vertexShaderCodigo = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        uniform vec2 uPosicao;
        uniform vec2 uTamanho;
        uniform vec2 uResolucao;

        void main() {
            vec2 posicao = aPos * uTamanho + uPosicao;
            vec2 clip = (posicao / uResolucao) * 2.0 - 1.0;
            clip.y = -clip.y;
            gl_Position = vec4(clip, 0.0, 1.0);
        }
    )";

    const char* fragmentShaderCodigo = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 uCor;

        void main() {
            FragColor = vec4(uCor, 1.0);
        }
    )";

    GLuint vertexShader = compilarShader(GL_VERTEX_SHADER, vertexShaderCodigo);
    GLuint fragmentShader = compilarShader(GL_FRAGMENT_SHADER, fragmentShaderCodigo);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int sucesso;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &sucesso);
    if (!sucesso) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "Erro ao linkar shader:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void prepararRetangulo() {
    float vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void desenharRetangulo(const Retangulo& r, float vermelho, float verde, float azul) {
    glUseProgram(shaderProgram);

    glUniform2f(glGetUniformLocation(shaderProgram, "uPosicao"), r.x, r.y);
    glUniform2f(glGetUniformLocation(shaderProgram, "uTamanho"), r.largura, r.altura);
    glUniform2f(glGetUniformLocation(shaderProgram, "uResolucao"), (float)LARGURA, (float)ALTURA);
    glUniform3f(glGetUniformLocation(shaderProgram, "uCor"), vermelho, verde, azul);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void desenharBarraDePontos() {
    int blocos = pontos;
    if (blocos > 20) blocos = 20;

    for (int i = 0; i < blocos; i++) {
        Retangulo bloco{20.0f + i * 18.0f, 20.0f, 14.0f, 14.0f};
        desenharRetangulo(bloco, 0.1f, 0.8f, 0.2f);
    }
}

void desenharVidas() {
    for (int i = 0; i < vidas; i++) {
        Retangulo vida{20.0f + i * 30.0f, 50.0f, 22.0f, 22.0f};
        desenharRetangulo(vida, 0.9f, 0.1f, 0.1f);
    }
}

void desenharFimDeJogo() {
    Retangulo fundo{200.0f, 220.0f, 400.0f, 120.0f};
    desenharRetangulo(fundo, 0.15f, 0.15f, 0.15f);

    Retangulo linha1{250.0f, 255.0f, 300.0f, 20.0f};
    Retangulo linha2{280.0f, 295.0f, 240.0f, 20.0f};

    desenharRetangulo(linha1, 0.9f, 0.1f, 0.1f);
    desenharRetangulo(linha2, 0.9f, 0.9f, 0.9f);
}

void renderizar() {
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    desenharRetangulo(jogador, 0.1f, 0.35f, 0.9f);

    for (const auto& obj : objetos) {
        if (obj.bonus) {
            desenharRetangulo(obj.corpo, 1.0f, 0.85f, 0.1f);
        } else {
            desenharRetangulo(obj.corpo, 0.9f, 0.1f, 0.1f);
        }
    }

    desenharBarraDePontos();
    desenharVidas();

    if (jogoAcabou) {
        desenharFimDeJogo();
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    if (!glfwInit()) {
        std::cout << "Erro ao iniciar GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    janela = glfwCreateWindow(LARGURA, ALTURA, "Catch Game 2D", nullptr, nullptr);
    if (janela == nullptr) {
        std::cout << "Erro ao criar janela GLFW.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(janela);
    glfwSetFramebufferSizeCallback(janela, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Erro ao iniciar GLAD.\n";
        glfwTerminate();
        return -1;
    }

    criarShader();
    prepararRetangulo();
    atualizarTitulo();

    float ultimoTempo = (float)glfwGetTime();

    while (!glfwWindowShouldClose(janela)) {
        float tempoAtual = (float)glfwGetTime();
        float deltaTempo = tempoAtual - ultimoTempo;
        ultimoTempo = tempoAtual;

        processarEntrada(deltaTempo);
        atualizarJogo(deltaTempo, tempoAtual);
        renderizar();

        glfwSwapBuffers(janela);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
