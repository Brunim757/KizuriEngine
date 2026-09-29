# Kizuri Engine — Documento de Design Técnico (TDD)

**Versão:** 0.1 (rascunho inicial)
**Data:** Setembro de 2026
**Autor:** Kizuri Studio

---

## Regras

1. Sem comentários em código
2. Build via GitHub — comando `gh` já configurado e vinculado à conta
3. Não compilar nem rodar nada localmente — este ambiente não é para isso
4. Repositório: `KizuriEngine`, público no GitHub (mesmo sendo a engine proprietária — licença de uso restrita, mas código visível)
5. Testes: tudo que **não depende de interface** deve ser testado no próprio GitHub (CI); o que **depende de interface** (Editor, Viewport, janelas) fica pendente até o autor testar localmente no PC e validar visualmente
6. Sempre **verificar os arquivos e o fluxo do código** antes de considerar uma tarefa concluída — revisar se a lógica está correta ponta a ponta, não só se compila. Um erro de uma única linha pode fazer algo parecer implementado mas não funcionar de fato (ex.: código que deveria renderizar um cubo, compila sem erro, mas por uma linha errada o cubo não aparece na tela)
7. **Proibido placeholder, stub ou implementação simplificada "pra depois".** Toda feature implementada precisa funcionar de ponta a ponta antes de ser considerada parte do sistema — isso inclui a exposição de componentes no Editor: se um sistema (Física, Animação, Scripting, etc.) ganha um componente novo no ECS, o desenho correspondente no Inspector é parte obrigatória da mesma entrega, não uma tarefa adiada para uma fase de "polimento" futura
8. **O CI deve entregar artefato de build desde a Fase 0**, não só reportar verde/vermelho. Todo pipeline do GitHub Actions publica o executável/binários compilados como artifact baixável (ex.: `actions/upload-artifact`), para que o autor consiga pegar e testar localmente (regra 5) sem precisar compilar do zero na própria máquina
9. **DEVE REVISAR TODOS OS ARQUIVOS QUE FORAM ADICIONADOS, MEXIDOS E VER SE A IMPLEMNTACAO ESTA CORRETA PARA NAO TER PROBELMAS
10. DEVE PESQUISAR A API DAS LIBS QUE FOR MEXER PARA ADICIONAR PERFEITAMNTE
11. DEVE ESTUDAR A PROPRIA ENGINE PARA ADICIONAR TUDO COM PERFEIÇÃO 
12. SEMPRE LER ESSE ARQUIVO E O ROADMAP ANTES DE COMECAR UMA FASE
13. SO INICIAR UMA FASE QUANDO A ANTERIOR ESTIVER 100%
14. E PARA FAZER OQUE ESTA DESCRITO AQUI NO TDD E NO ROADMAP NAO ALGO SIMPLES EQUIVALENTE FAZER ALGO SIMPLES ESTA ESTRITAMENTE PROIBIDO EM UMA GAME ENGINE VC DEVE FAZER TUDO O MAIS AVANCADO E COMPLETO POSSIVEL NAO ALGO SIMPLES QUE FUCNIONA EU QUERO EXATAMNTE OQUE O TDD E O ROADMAP FALAM SEM SIMPLIFICAR SO PARA ENTREGAR MAIS RAPIDO EU NAO LIGO PARA O TEMPO QUE VAI DEMORAR EU NAO LIGO PARA O TAMANHO QUE VAI FICAR MAS TEM QUE FEITO DE VERDADE SIMPLIFCAR NAO É FAZER BEM FEITO
---

## 1. Visão geral

A **Kizuri Engine** é uma game engine 3D desenvolvida do zero em C++, com scripting em C# (incluindo scripting visual por nós compilado via Roslyn), voltada para **Action RPGs souls-like** (referências: Dark Souls, Elden Ring, Sekiro).

A engine é **proprietária e fechada**: não haverá marketplace de plugins, não haverá acesso ao código-fonte C++ por terceiros, e o uso é restrito ao autor e a parceiros autorizados (atualmente, um estúdio parceiro que pretende usar a engine para um projeto próprio, sem envolvimento no desenvolvimento da engine em si).

### 1.1 Motivação

Este é um projeto pessoal, motivado por interesse genuíno em engenharia de motores de jogo e pelo gênero souls-like — não uma resposta a demanda de mercado ou de terceiros. Decisões de escopo priorizam profundidade técnica no nicho escolhido em vez de abrangência genérica (evitando repetir o posicionamento "faz tudo" de engines como Unity/Unreal).

### 1.2 Filosofia central

- **Performance é requisito não-negociável.** A engine deve suportar jogos grandes e visualmente ricos sem engasgos (stutter), travamentos de interface ou quedas de frame perceptíveis.
- **Padrão arquitetural repetido:** a engine fornece sistemas robustos e configuráveis; o jogo (via C#/scripting visual) decide o comportamento e o tuning. A engine nunca impõe decisões de design de gameplay.
- **Fechada e pragmática:** sem preocupação de portabilidade multiplataforma, sem necessidade de suportar todo tipo de jogo — o foco no nicho souls-like é o que permite profundidade técnica real.

### 1.3 Nicho e características do gênero-alvo

O gênero souls-like impõe requisitos técnicos específicos que a engine deve suportar nativamente ou facilitar fortemente:

- Combate baseado em stamina, timing e "commitment" de animação (não cancelável a meio caminho)
- Boss fights com IA de padrões e fases
- Level design interconectado e vertical, com atalhos
- Sistema de morte/checkpoint com perda/recuperação de recurso
- Renderização fotorrealista (PBR) com atmosfera dramática (fog, iluminação direcional forte)
- Câmera em terceira pessoa com lock-on de combate

---

## 2. Arquitetura geral

### 2.1 Estrutura de build

- **1 executável final** por jogo, gerado pelo sistema de Build do editor
- **Núcleo sempre presente**: sistemas que todo jogo feito na Kizuri sempre usa (`KizuriRender`, `KizuriPhysics`, `KizuriAnimation`, `KizuriAudio`, `KizuriScripting`, `KizuriCore` — ECS, job system, alocadores) são compilados como `.lib` estáticas **internamente, pelo Kizuri Studio**, e linkadas dentro de um único executável "stub" pré-compilado: `KizuriRuntime.exe`. As `.lib` em si **nunca são distribuídas** — são um artefato interno do processo de build da própria engine, sem utilidade para quem não tem um linker C++ (e o SDK não expõe compilação C++ a ninguém, por decisão já registrada na seção sobre o SDK)
- **Sistemas opcionais, como `.dll` pré-compiladas com carregamento tardio**: sistemas que nem todo jogo precisa — hoje, o caso claro é `KizuriNetworking.dll`; outros módulos opcionais que surgirem no futuro seguem o mesmo padrão

**Correção arquitetural (v0.4):** versões anteriores deste documento cogitaram distribuir `.lib` diretamente ou gerar múltiplos executáveis por combinação de módulos — ambas descartadas. A primeira não faz sentido porque o estúdio que usa a engine nunca tem um toolchain de compilação C++ para consumir uma `.lib`; a segunda gera explosão combinatória conforme módulos opcionais aumentam. A solução final combina o melhor dos dois problemas resolvidos: **um único stub pré-compilado para o núcleo (sempre igual) + DLLs individuais só para o que é genuinamente opcional**.

**O que o SDK distribui de fato:**
- `KizuriRuntime.exe` — o stub com o núcleo já linkado, pronto para receber assets e C# de qualquer projeto
- As `.dll` de sistemas opcionais, pré-compiladas
- Editor, Launcher

**O que o Build faz, dentro do Editor, sem nenhuma compilação C++:**
1. Copia o `KizuriRuntime.exe` e renomeia para o nome do jogo (ex.: `MeuJogo.exe`)
2. Empacota os assets e o C# compilado do projeto como dados carregados em runtime pelo stub (não como algo linkado)
3. Analisa quais sistemas opcionais o projeto referencia em C# e copia as `.dll` correspondentes para a mesma pasta

Exemplo prático:
```
SDK distribuído (instalador):
├── Editor.exe / Launcher.exe
└── Runtime/
    ├── KizuriRuntime.exe       (núcleo já linkado, stub pronto)
    └── KizuriNetworking.dll    (pré-compilada, copiada só se usada)

Build de um projeto sem rede:
MeuJogo/Build/
├── MeuJogo.exe                 (cópia renomeada do KizuriRuntime.exe)
└── Data/                       (assets + C# compilado do projeto)

Build de um projeto com rede:
MeuJogo/Build/
├── MeuJogo.exe
├── Data/
└── KizuriNetworking.dll        (copiada automaticamente pelo Build)
```

### 2.2 Camadas da engine

```
┌─────────────────────────────────────────┐
│   Gameplay (C# + Scripting Visual/Nós)   │  ← Dev do jogo trabalha aqui
├─────────────────────────────────────────┤
│   API C# (assemblies expostas)           │  ← Contrato entre C# e C++
├─────────────────────────────────────────┤
│   Core Engine (C++, ECS híbrido)         │  ← Renderer, física, áudio,
│   Job System + Alocadores próprios       │    animação, scripting host
└─────────────────────────────────────────┘
```

- **Núcleo em C++**: sistemas de baixo nível, performance crítica (renderer, física, job system, alocadores)
- **ECS híbrido**: dados organizados como Entity Component System (cache-friendly, paralelizável via job system) por baixo, mas exposto em C# através de uma API que se parece com componentes tradicionais — o dev não precisa entender ECS para usar a engine
- **Scripting**: toda lógica de gameplay é escrita em C# ou via scripting visual por nós (que compila para C# real via Roslyn, não é interpretado em runtime)

### 2.3 Plataforma e API gráfica

- **Plataforma alvo: Windows apenas** (sem necessidade de portabilidade)
- **API gráfica: DirectX 11**
- **RHI (Render Hardware Interface)**: camada fina de abstração entre o renderer e a API gráfica (`CreateBuffer()`, `CreateTexture()`, `Draw()`, etc.). O renderer nunca chama DX11 diretamente — chama a RHI, que hoje traduz para DX11 e no futuro pode ganhar um backend DX12/Vulkan sem exigir reescrita do renderer. **Decisão arquitetural obrigatória desde o início**, dado o alto custo de adicionar isso retroativamente.

### 2.4 SDK e distribuição

- SDK **100% baseado em C#/scripting visual** — nenhuma exposição de C++ nativo a terceiros
- Distribuição via **instalador completo** (Editor + Launcher + runtime .NET necessário), registra associação de arquivo `.kzproj`
- Conteúdo do SDK: Editor, Launcher, assemblies C# (contrato de API), documentação (gerada dos comentários XML do C#), exemplos

---

## 3. Stack de bibliotecas de terceiros

Uma biblioteca definitiva por categoria — sem alternativas em aberto — todas com licença permissiva compatível com engine proprietária fechada.

| Categoria | Biblioteca | Licença |
|---|---|---|
| Física | Jolt Physics | MIT |
| Áudio | miniaudio | MIT / Public Domain |
| Scripting C# | Roslyn (Microsoft.CodeAnalysis.CSharp) | MIT |
| Host .NET/CoreCLR | nethost / hostfxr | MIT |
| Matemática | DirectXMath | MIT |
| Import de modelos 3D | cgltf | MIT |
| Import de texturas | stb_image | Public Domain |
| Compressão/mipmap de textura | DirectXTex | MIT |
| Job System (multithread) | enkiTS | zlib |
| Rede P2P | GameNetworkingSockets | BSD-3-Clause |
| Editor/UI de ferramentas | Dear ImGui | MIT |
| Gizmo de transformação | ImGuizmo | MIT |
| Ícones do editor | Fork Awesome + IconFontCppHeaders | MIT / OFL |
| Fontes/texto | FreeType | FTL/BSD-like |
| Serialização (save/assets) | FlatBuffers | Apache 2.0 |
| Compressão de assets | zstd | BSD-3-Clause |
| Janela/Input | Win32 API nativo | (sistema operacional) |

---

## 4. Sistemas centrais (Core Engine)

### 4.1 Renderização

- **Pipeline: Deferred Rendering** — suporta dezenas/centenas de luzes dinâmicas sem degradar performance, necessário para cenas fotorrealistas com iluminação complexa
- **Sombras**: Cascaded Shadow Maps (CSM) para luz direcional + shadow maps comuns para luzes pontuais/spot, com PCF/PCSS para suavização
- **Upscaling**: suporte a FSR (AMD, agnóstico de fabricante — DLSS é exclusivo Nvidia)
- **Path tracing: fora de escopo** — inviável em tempo real sem Ray Tracing por hardware, que DX11 não suporta nativamente
- **Materiais**: PBR (albedo, roughness, metallic, normal map)

### 4.2 Física — Jolt Physics

- Colisão, rigidbody, joints, ragdoll
- **Padrão arquitetural**: a engine fornece o sistema de ragdoll/joints configurável; o comportamento final (rigidez, o que quebra, como reage) é decisão do jogo, não da engine
- Destruição de cenário: possível via Jolt, mas configuração/uso é responsabilidade do jogo

### 4.3 Áudio

- Base: miniaudio (device I/O + decodificação)
- Camada de áudio 3D posicional própria, construída sobre o miniaudio

### 4.4 Animação

- Sistema de animação "estado da arte": blending de múltiplas camadas (locomotion + upper body + aim)
- **IK completo**: pés ajustados ao terreno, mãos em objetos, look-at de cabeça/tronco
- Root motion preciso (essencial para golpes, dashes, animações de combate com "commitment")
- Editor de state machine / blend tree no Editor

### 4.5 IA e Pathfinding

- NavMesh dinâmico (recalcula com obstáculos móveis)
- Hierarchical pathfinding (performance em mundos grandes)
- Comportamento de squad/group avoidance (múltiplos inimigos não se atropelam)

### 4.6 Rede

- Modelo: **P2P com host migration** (mesmo modelo usado por Dark Souls/Elden Ring), sem necessidade de servidor dedicado
- Biblioteca: GameNetworkingSockets

### 4.7 VFX

- Sistema de partículas **GPU-based**, suportando milhares de partículas sem impacto perceptível de performance

### 4.8 Câmera

- Sistema de câmera terceira pessoa **built-in**: raycast/colisão roda em C++ (performance), exposto como componente configurável via C# (modelo similar à Cinemachine da Unity) — todo jogo ganha uma câmera funcional por padrão, mas pode sobrescrever
- **Lock-on de combate NÃO é responsabilidade nativa da engine** — fica a cargo do jogo, por ser uma decisão de design de combate

### 4.9 Sistema de Prefabs

- **Prefab** é um asset próprio (`.kzprefab`) que guarda um "molde" de entidade (ou hierarquia de entidades), serializado pela mesma capacidade de serialização genérica por reflection descrita na seção 4.10
- Instâncias colocadas numa cena referenciam o Prefab original; alterar o Prefab **propaga automaticamente** para todas as instâncias existentes
- **Overrides por instância**: cada instância pode sobrescrever propriedades específicas sem perder a ligação com o Prefab original; um override sobrevive a atualizações futuras do Prefab
- **Apply/Revert**: uma instância pode empurrar sua mudança de volta ao Prefab original (propagando a todas as outras instâncias) ou descartar o override (voltando ao valor do Prefab)
- **Prefab Variants**: um Prefab pode herdar de outro, sobrescrevendo apenas parte de suas propriedades (ex.: variações de um mesmo tipo de inimigo)
- **Modo de edição isolado**: editar um Prefab abre um contexto próprio, fora de qualquer cena, evitando alterar sem querer apenas uma instância específica
- Instanciável tanto via API C# quanto por nó de Scripting Visual — é o mecanismo central para popular um nível souls-like com inimigos e itens repetidos sem duplicação manual de trabalho

### 4.10 Save System (do jogo)

- Sistema de **serialização genérica baseada em reflection** (qualquer componente/entidade pode ser serializado automaticamente) — construída cedo, já na fundação do Editor (Roadmap, Fase 3), e reaproveitada por quatro consumidores diferentes: "Salvar Cena" do Editor, snapshot de Play-in-Editor, Sistema de Prefabs, e o Save System do jogo descrito aqui
- Gerenciador de save slots (múltiplos saves, autosave), exposto ao jogo via C#
- O conceito de "checkpoint"/bonfire (o que é salvo, quando) é decisão do jogo, construída sobre a serialização fornecida pela engine
- **Não deve ser confundido com "Salvar Cena" do Editor** — são consumidores diferentes da mesma capacidade de serialização: um persiste o estado de autoria (a cena que o dev está montando), o outro persiste o progresso do jogador em runtime

### 4.11 Input

- Rebind completo de teclas/botões
- Suporte a múltiplos dispositivos de controle simultâneos

### 4.12 Threading e memória — requisito não-funcional crítico

- **Job System próprio** (enkiTS) para paralelizar streaming, física, animação e outras cargas pesadas
- **Alocadores customizados** (pool allocators, arena/stack allocators por sistema) — evitar `malloc`/`new` no hot path, minimizar fragmentação
- **Regra obrigatória**: nenhuma operação de I/O ou processamento pesado roda na thread principal/UI. Toda operação pesada passa pelo job system, com comunicação assíncrona de volta à interface via fila de eventos. Isso é o que garante que o Editor (e o jogo) nunca travem a interface durante carregamentos — requisito citado explicitamente como problema em projetos anteriores do autor e que a Kizuri deve resolver estruturalmente.

### 4.13 Localização

- Apenas inglês no lançamento inicial
- Boas práticas desde já: todo texto passa por um sistema de "string por chave" (mesmo com um idioma só), para não travar expansão futura

### 4.14 Modding

- Não é responsabilidade nativa da engine (decisão do jogo/estúdio que a utiliza)
- Arquitetura não deve *impedir* modding futuro, mas não é requisito ativo

---

## 5. Editor

### 5.1 Aparência e layout

- **Tema dark** por padrão (ferramenta profissional, uso prolongado)
- **Menu Bar** (File, Edit, Assets, Entity, Window, Help) acima da **Toolbar** (Play, Save, Settings)
- Estrutura de painéis dockable e redimensionáveis, podendo ser destacados para outra tela:
  - **Hierarquia** (árvore de entidades da cena)
  - **Viewport 3D** com gizmos de transformação e Play-in-Editor
  - **Inspector** (componentes da entidade selecionada)
  - **Asset Browser** (modelos, texturas, áudio, scripts)
  - **Console** (logs)
  - **Editor de Terreno/Vegetação** (sculpting manual via heightmap + pintura de vegetação com instancing — sem geração procedural)
  - **Editor de Materiais**
  - **Editor de Scripting Visual** (nós)
  - **Editor de Animação** (state machine, blend tree, preview de IK)
  - **Editor de VFX/Partículas**
  - **Profiler visual** (CPU/GPU/memória, frame time por sistema)
  - **Debugger de física** (colliders, ragdoll, raycasts)
  - **Debugger de animação** (esqueleto, IK, blend weights)
  - **Painel de Documentação** (dockable, navegável dentro do editor — ver seção 5.6)

### 5.2 Atalhos de teclado

Conjunto padrão de produtividade, customizável em "Edit → Atalhos de Teclado":

- `Ctrl+S` salvar cena · `Ctrl+Z/Y` desfazer/refazer
- `Ctrl+D` duplicar entity · `Delete` deletar
- `F` focar câmera no objeto selecionado
- `W/E/R` mover/rotacionar/escalar (gizmo)
- `Ctrl+P` entrar/sair do Play Mode
- `F1` abrir documentação contextual

### 5.3 Drag and drop

Requisito funcional obrigatório em todo o editor:

- Arrastar `.glb` do Asset Browser para o Viewport → cria Entity com Mesh Renderer configurado
- Arrastar textura para um slot de material no Inspector → atribui a textura
- Arrastar script C#/nó para uma Entity → adiciona como componente
- Arrastar Entity sobre outra na Hierarquia → parenting
- Campos de referência no Inspector aceitam arrastar Entities/assets compatíveis

### 5.4 Scripting Visual (nós)

- Modelo de **execução por fios**, no estilo Unreal Blueprint: nós conectados por **pinos de execução** (fio de ordem/fluxo) e **pinos de dados** (fio tipado, carrega valores)
- Categorias de nós: Eventos (`OnBeginPlay`, `OnTakeDamage`, `OnCollision`), Ações (`PlayAnimation`, `ApplyDamage`, `SpawnEntity`), Fluxo de controle (`Branch`, `Sequence`, `ForLoop`, `Delay`), Variáveis, Funções customizadas (sub-grafos reutilizáveis)
- **Diferencial técnico**: ao salvar, o grafo é traduzido para **C# real e legível**, compilado pelo Roslyn — não é interpretado nó-por-nó em runtime como a maioria dos visual scriptings. Resultado: performance de C# nativo.
- Debug: visualização de valores fluindo pelos pinos em tempo real durante Play Mode, breakpoints em nós
- Escopo: editor visual serve para **qualquer lógica de gameplay**, não apenas IA de boss

### 5.5 Sistema de validação contextual (avisos)

Inspirado no modelo do Godot:

- Ícone de aviso (⚠) na Hierarquia quando uma Entity tem configuração incompleta/inconsistente
- Banner de aviso no Inspector explicando o problema (ex.: "Mesh Renderer sem material atribuído")
- Avisos também registrados no Console
- Comando "Validar Cena" roda a checagem em todas as Entities de uma vez
- Cada componente define suas próprias regras de validação — padrão arquitetural extensível conforme novos sistemas são adicionados

### 5.6 Documentação

- **Fonte única**: gerada a partir dos comentários XML (`/// <summary>`) do próprio código C# da API
- **Dentro do editor**: painel dockable de documentação completa e pesquisável (estilo Godot), navegação por links cruzados entre tipos, atalho `F1` contextual (clicar num nó/tipo abre a página de referência correspondente)
- **Fora do editor**: mesmo conteúdo publicado como site estático, para consulta sem abrir o editor ou compartilhamento de link

### 5.7 Sistema de Build

- Configuração de build dentro do Project Settings
- Botão "Build" gera o executável final em 1 clique, com linkagem seletiva das `.lib` conforme uso real do projeto (ver seção 2.1)

### 5.8 UI (HUD e menus do jogo)

- Dear ImGui: exclusivo para ferramentas internas do editor
- Framework de UI próprio (retained mode): toolkit que o dev usa para construir a UI do jogo final (HUD, menus) — não é uma UI pronta, é a ferramenta de construção

---

## 6. Launcher e fluxo de criação de projeto

### 6.1 Kizuri Launcher

Aplicativo separado do Editor, executado antes dele:

- Lista de projetos recentes
- Botão "Novo Projeto" (sempre cria projeto em branco — sem templates)
- Gerencia versão da engine instalada, evitando incompatibilidade entre projeto e engine

### 6.2 Estrutura gerada ao criar um projeto

```
MeuJogo/
├── Assets/          (modelos glTF, texturas, áudio)
├── Scripts/         (C# puro + grafos de nós)
├── Scenes/
├── Config/
├── Build/           (saída do sistema de build)
└── MeuJogo.kzproj   (aponta para a versão da engine usada)
```

### 6.3 Preferências do editor (layout de painéis)

- Layout de painéis (equivalente ao `imgui.ini`) **não fica salvo dentro da pasta do projeto**
- Salvo por usuário, em local padronizado pelo instalador (ex.: `%APPDATA%/Kizuri/EditorLayout.ini`)
- Resultado: a pasta do projeto permanece limpa, contendo apenas Assets/Scripts/Scenes/Config; o layout é preferência pessoal do dev, não algo versionado no projeto

---

## 7. Requisitos de hardware (estimativa inicial)

**Metodologia**: como não é viável testar fisicamente em todo hardware existente, a estimativa segue a prática padrão da indústria:

1. Uso do Steam Hardware Survey como base do que a maioria dos jogadores realmente possui
2. Cálculo de orçamento de frame (16,6ms para 60fps), dividido entre renderer, física, animação etc.
3. Definição de uma "vertical slice" (cena representativa) testada em GPUs de referência de cada faixa de mercado
4. Extrapolação matemática entre pontos de referência usando benchmarks públicos (TechPowerUp, etc.)
5. Refinamento pós-lançamento via telemetria real de uso

**Esta tabela é uma meta de design inicial**, a ser validada quando existir conteúdo de jogo real (vertical slice) para benchmark:

| | Mínimo | Recomendado |
|---|---|---|
| GPU | GTX 1060 6GB / RX 580 (com FSR ativo) | RTX 3060 / RX 6600 |
| CPU | 4 núcleos / 8 threads (ex.: i5-8400) | 6 núcleos / 12 threads (ex.: Ryzen 5 5600) |
| RAM | 8 GB | 16 GB |
| Armazenamento | SSD (essencial para streaming sem stutter) | SSD NVMe |
| Alvo de resolução/FPS | 1080p / 30fps com FSR | 1080p / 60fps ou 1440p com FSR |

O requisito mínimo de 4 núcleos reais é arquitetural, não arbitrário: é o piso necessário para separar efetivamente a thread de UI do trabalho pesado via job system (seção 4.11).

---

## 8. Riscos Técnicos e Decisões de Mitigação

Esta seção documenta armadilhas de baixo nível identificadas por revisão técnica externa, que não são visíveis ao planejar a engine em alto nível, mas que comprometem a arquitetura se ignoradas na fundação. Cada risco tem uma decisão de mitigação já registrada, para ser aplicada na fase correspondente do Roadmap.

### 8.1 Fronteira C++/C# e Gerenciamento de Memória

- **Marshalling no hot path**: chamadas P/Invoke repetidas milhares de vezes por frame (ex.: posição de cada entidade) destroem performance. **Mitigação**: usar Blittable Types e ponteiros brutos (`void*`/`IntPtr`, `unsafe`/`ref struct`) para o C# ler/escrever direto na memória do ECS em C++, sem cópia na fronteira.
- **GC do .NET causando stutter**: alocação dinâmica (`class`, strings) na API C# dispara coletas de 10-50ms. **Mitigação**: API C# exposta pela engine usa apenas `struct`/tipos value, evitando alocação no hot path; pooling para qualquer alocação inevitável.
- **Alinhamento de memória (Blittable Types)**: `struct` em C++ e C# precisam ter layout de bytes idêntico, ou os dados corrompem na leitura. **Mitigação**: todo tipo compartilhado na fronteira C++/C# usa `[StructLayout(LayoutKind.Sequential)]` explícito e é validado por teste automatizado de tamanho/offset no CI.
- **Referências a entidades destruídas**: script C# segurando referência a uma Entity já deletada no C++ causa Access Violation. **Mitigação**: referências de Entity expostas ao C# usam um sistema de handle com geração (generational index), não ponteiro cru — acessar um handle inválido retorna erro controlado, não crash.
- **Fragmentação de heap por Hot-Reload**: recarregar assemblies via Roslyn sem limpar referências antigas infla a RAM continuamente. **Mitigação**: cada hot-reload usa um novo `AssemblyLoadContext` colecionável (`isCollectible: true`), com descarregamento explícito do contexto anterior antes de carregar o novo.

### 8.2 Concorrência e Escalonamento de Threads

- **Guerra de threads (over-subscription)**: Jolt Physics tem job system próprio; rodar em paralelo ao enkiTS sem coordenação gera context switching excessivo. **Mitigação**: implementar a interface `Jolt::JobSystem` como um adaptador que submete os jobs do Jolt diretamente ao enkiTS — um único scheduler para toda a engine (ver Fase 6 do Roadmap).
- **Condições de corrida no ECS híbrido**: script C# lendo um componente enquanto a física em C++ escreve nele corrompe dados. **Mitigação**: acesso a componentes segue o modelo de jobs do ECS (leitura/escrita declarada por sistema), com o scheduler garantindo que sistemas com dependência de dados não rodem simultaneamente sobre o mesmo componente.
- **Starvation da thread principal por I/O**: descompressão zstd em massa pelo Job System pode congestionar o SSD e travar a thread principal esperando sincronização. **Mitigação**: fila de I/O com prioridade e limite de operações concorrentes, para não saturar o barramento de disco de uma vez.
- **Inversão de prioridade no enkiTS**: job de baixa prioridade retendo um mutex que um job de alta prioridade (render do frame atual) precisa. **Mitigação**: jobs críticos de frame (render, input) evitam mutex compartilhado com jobs de baixa prioridade; usar estruturas lock-free onde a dependência for inevitável.

### 8.3 Renderização, RHI (DX11) e Streaming de Assets

- **Bloqueio de frame por upload síncrono à GPU**: enviar texturas/meshes carregadas em background via `UpdateSubresource`/`Map`/`Unmap` pode travar a thread principal na hora do envio. **Mitigação**: uploads de GPU passam por uma fila assíncrona com staging buffers, distribuídos ao longo de vários frames em vez de um upload bloqueante único.
- **Stall de pipeline por readback de GPU**: ler um pixel da GPU (ex.: seleção de entidade por clique no Viewport) força a CPU a esperar toda a renderização terminar. **Mitigação**: readback usa buffers com N frames de atraso (a CPU lê o resultado do frame anterior, não do atual), evitando sincronização bloqueante.
- **Redundância de estado na RHI**: a camada fina pode reenviar Render States já ativos na GPU. **Mitigação**: a RHI mantém um cache do último estado enviado e descarta chamadas redundantes antes de repassar ao driver.
- **Gargalo de Draw Calls no Deferred Rendering**: cenários souls-like densos podem estourar o limite de draw calls do DX11 na CPU. **Mitigação**: instancing agressivo para geometria repetida (vegetação, props) e culling de oclusão obrigatório desde a Fase 5 do Roadmap, não como otimização posterior.
- **Falta de VRAM sem Texture Streaming**: mundos interconectados sem telas de carregamento estouram os 6GB da GTX 1060 (requisito mínimo). **Mitigação**: sistema de streaming de textura por distância (mip reduzido para objetos distantes), integrado ao pipeline de assets da Fase 4.

### 8.4 Física (Jolt), Animação e Gameplay

- **Desalinhamento temporal entre física e renderização**: física em passo fixo (ex.: 60Hz) e renderização em taxa variável causam tremor visual. **Mitigação**: interpolação de posição/rotação do collider entre o último e o penúltimo passo de física, aplicada só na renderização (a simulação em si permanece no passo fixo).
- **Penetração de colisão em plataformas móveis**: Root Motion sobre elevadores rápidos pode atravessar o chão ou esmagar contra o teto. **Mitigação**: velocidade da plataforma é aplicada *antes* do cálculo de colisão do Character Controller no mesmo passo de física, não depois.
- **Latência de hitbox (frame de defasagem)**: um frame de atraso entre a animação do osso e o posicionamento do collider causa ataques fora de sincronia. **Mitigação**: colliders de ataque (hitboxes) são atualizados dentro do mesmo passo de animação, antes do passo de física do frame, não no frame seguinte.
- **Erro de arredondamento em mundos grandes (float 32-bit)**: precisão do DirectXMath degrada longe da origem, causando trepidação e falhas de colisão. **Mitigação**: sistema de "Origin Rebasing" — o mundo é reancorado periodicamente perto do jogador (a origem (0,0,0) se move com o personagem, e tudo mais é recalculado em relação a ela), evitando coordenadas absolutas muito grandes.

### 8.5 Engenharia do Editor

- **Corrupção de estado no Play-in-Editor**: rodar o jogo no mesmo espaço de memória do editor pode destruir a cena original ao editar scripts. **Mitigação**: o estado da cena é serializado (via o mesmo sistema de save da seção 4.9) antes de entrar em Play Mode, e restaurado automaticamente ao sair — não é um reload do arquivo do zero, é snapshot em memória.
- **Condição de corrida no Inspector (Immediate Mode)**: editar um valor no ImGui enquanto o Job System altera o mesmo dado em background gera conflito de escrita. **Mitigação**: edições feitas pelo Inspector são enfileiradas e aplicadas no início do frame seguinte, no mesmo ponto de sincronização que o restante do ECS usa — nunca escrita direta e imediata durante o frame.
- **Assets binários (FlatBuffers) incompatíveis com merge no Git**: cenas/terrenos binários impedem merge de branches. **Mitigação**: não é um problema ativo no momento (fluxo de trabalho é de um único desenvolvedor), mas registrado como risco latente — se o projeto ganhar mais de um colaborador direto no futuro, será necessário um formato intermediário textual (ex.: JSON) para cenas, convertido para binário só no build.
- **Estouro de RAM no Undo/Redo**: salvar cópias completas de componentes a cada alteração de mouse (pintura de vegetação, sculpting) esgota memória rápido. **Mitigação**: histórico de undo usa diffs incrementais (delta de mudança), não snapshots completos, com compactação periódica do histórico antigo.
- **Travamento por recompilação do Roslyn a cada Ctrl+S**: compilar o grafo inteiro no disco a cada salvamento trava a interface por segundos. **Mitigação**: compilação incremental (só o(s) grafo(s) modificado(s)) rodando em background pelo Job System, sem bloquear a UI; o editor mostra estado "compilando" sem travar.

### 8.6 IA, Rede e Sistemas Auxiliares

- **Gargalo de CPU em NavMesh 3D vertical**: recalcular NavMesh dinamicamente em áreas com múltiplos andares sobrepostos (comum em souls-like) derruba o frame rate. **Mitigação**: recalculo de NavMesh é particionado em regiões (tiles) e só a região afetada por uma mudança (ex.: portão abrindo) é recalculada, nunca o mundo inteiro.
- **Divergência de estado em rede P2P**: sem previsão de cliente e rebobinamento, perda de pacotes causa descompasso entre o que o jogador vê e o que o host aplica. **Mitigação**: client-side prediction com reconciliação (rollback) no sistema de rede, a ser detalhado tecnicamente na Fase 15 do Roadmap antes da implementação.
- **Latência de input pela fila de mensagens do Win32**: processar esquiva dentro do loop padrão `GetMessage` expõe o input a oscilações do SO/renderização. **Mitigação**: leitura de input crítico (esquiva, ataque) via polling de estado de baixo nível (ex.: `GetAsyncKeyState`/Raw Input) fora do loop de mensagens do Win32, não dependente da fila de eventos da janela.

---

## 9. Fora de escopo (v1)

Itens conscientemente excluídos do escopo inicial:

- Multiplataforma (console, Linux, macOS)
- Ray tracing / path tracing (limitação de DX11)
- Servidores dedicados
- Marketplace de plugins/assets de terceiros
- Templates de projeto no Launcher
- Localização multi-idioma (apenas preparação arquitetural)
- Modding com suporte nativo da engine
- Hot-reload de código C++

---

## 10. Próximos passos sugeridos

1. Validar a arquitetura da RHI e do job system com um protótipo mínimo (renderer + física + 1 luz dinâmica)
2. Definir prioridade de implementação dos módulos do Core Engine
3. Construir a primeira "vertical slice" para validar os requisitos de hardware estimados na seção 7
4. Especificar em detalhe o formato do arquivo `.kzproj` e do pipeline de assets (glTF → formato interno)
