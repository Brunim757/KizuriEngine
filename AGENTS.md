# KIZURI ENGINE — GAME ENGINE DESIGN DOCUMENT (GDD Técnico)
### Engine 3D AAA Open-World Genérica — Gênero Nível Genshin Impact
**Stack:** C++20 / DirectX 11 / Solo Dev / CI via GitHub Actions

---

## 0. VISÃO E PILARES

**Visão:** Construir a melhor engine de código aberto (ou privada) do mundo especializada no **gênero** de RPGs open-world com estética estilizada/anime, cel-shading de alta qualidade e streaming de mundo massivo — sem depender de Unreal ou Unity.

**Importante — escopo do projeto:** a Kizuri Engine é feita para suportar jogos no estilo Genshin Impact (open-world, ação, estética anime/cel-shading, mundo vivo). Isso significa que os sistemas devem ser flexíveis o bastante pra esse gênero — não hardcoded pra uma única cena ou personagem específico — mas o foco é este gênero, não virar uma engine genérica multi-propósito estilo Unreal.

**Pilares inegociáveis:**
1. **Performance-first** — toda decisão de arquitetura passa pelo filtro "isso escala pra 500 NPCs + streaming de 10km²?"
2. **Data-Oriented Design** — ECS puro, cache-friendly, zero "GameObject God Class"
3. **Ferramentas de produtividade** — Editor e Project Hub são cidadãos de primeira classe, não um afterthought
4. **Código sem comentários** — nomenclatura autoexplicativa, funções pequenas, tipos fortes (comentários só em `.md`/docs externos, nunca inline)
5. **CI obrigatória desde o commit #1** — se não builda no GitHub Actions, não é merge
6. **Zero placeholder/stub** — nenhum módulo existe antes de ser realmente implementado
7. **Fases são sequenciais e travadas** — só se avança pra próxima fase do roadmap quando a atual estiver 100% concluída; nenhum sistema de uma fase futura é criado antecipadamente, mesmo como esqueleto
8. **Pesquisa profunda antes de qualquer implementação** — nenhuma lib nova é adicionada sem pesquisar a fundo como ela funciona, como integrar corretamente e quais são as pegadinhas comuns de uso (docs oficiais, exemplos reais, issues conhecidas); nenhuma mudança de código no engine (incluindo configuração de CI/GitHub Actions) é feita sem estudar antes a forma correta de implementar. Zero tentativa-e-erro direto no código principal — a pesquisa vem antes do commit, não depois do erro
9. **Nenhuma compilação local nao adianta ver se tem ferramenta nao e para compilar nada**
---

## 1. STACK TECNOLÓGICO

### 1.1 Core
| Categoria | Escolha | Motivo |
|---|---|---|
| Linguagem | C++20 | Concepts, coroutines (streaming assíncrono), modules (opcional) |
| Render API | DirectX 11 | Maturidade, debug tooling (PIX, RenderDoc), portar pra DX12 depois |
| Build system | CMake + Ninja | Padrão de mercado, integra bem com CI |
| Compilador | MSVC (clang-cl como alternativa) | |

### 1.2 Bibliotecas (permitido usar centenas — curadoria por domínio)
**Regra de adoção (Pilar 8):** antes de integrar qualquer lib abaixo (ou trocar por outra), pesquisa profunda obrigatória: documentação oficial, exemplos de integração reais, issues/limitações conhecidas — só depois disso começa a implementação.
- **Matemática:** DirectXMath
- **Física:** Jolt Physics (moderna, multithread nativa, usada pela Horizon Forbidden West)
- **Áudio:** miniaudio (MIT license, zero custo/royalty, single-header, produção-ready)
- **Animação:** ozz-animation
- **Networking (futuro co-op):** GameNetworkingSockets (Valve)
- **Scripting (código):** C# via CoreCLR hosting (**.NET 10**, LTS) + Roslyn para compilação/hot-reload em runtime
- **Scripting (visual):** KizuriNodes — editor de nós próprio (ImNodes), transpila para C# e roda no mesmo runtime
- **Serialização:** flatbuffers
- **ECS:** EnTT (referência de mercado, extremamente rápida)
- **Job System:** próprio, inspirado no Fiber-based Job System da Naughty Dog
- **UI do Editor:** Dear ImGui + ImGuizmo + ImNodes
- **Ícones do Editor:** Lucide (icon set moderno, stroke-based, extremamente consistente visualmente, MIT license) — exportado como icon font + IconFontCppHeaders para uso direto no ImGui
- **UI do Jogo (HUD):** RmlUi (renderiza HTML/CSS, usado em jogos AAA como Detroit: Become Human, open-source)
- **Asset import:** Assimp (modelos), DirectXTex (texturas)
- **Compressão de texturas:** DirectXTex (BC7, BC5 normal maps)
- **Virtual Texturing:** implementação própria sobre sparse textures (D3D11 tiled resources)
- **Profiling:** próprio (KizuriProfiler), instrumentação CPU (timers por escopo, thread-aware) + GPU (queries D3D11 timestamp), visualizado no Editor
- **Logging:** spdlog
- **Testes:** Catch2
- **Hot-reload de shaders:** próprio, com file watcher (efsw)
- **Ferramenta de apoio à autoria de terreno:** FastNoise2 (usado só como ponto de partida no Editor — o artista esculpe/ajusta manualmente depois, o terreno final é um asset estático, não gerado em runtime)
- **Compressão geral:** zstd
- **Formato de cena:** glTF 2.0 como intermediário
- **VFX/Partículas:** Effekseer (open-source, feito no Japão especificamente pra efeitos estilo anime, usado em jogos comerciais reais, editor de efeitos próprio integrável)

### 1.3 CI/CD (GitHub Actions)
**Regra de adoção (Pilar 8):** qualquer mudança em pipeline/workflow do GitHub Actions passa por estudo prévio da sintaxe/comportamento correto antes de commitar — nada de ajustar `.yml` por tentativa-e-erro até "passar verde".
- Pipeline `build.yml`: matrix build (Debug/Release) a cada push
- Pipeline `test.yml`: roda testes unitários (Catch2) + testes de regressão de renderização via **checksum de buffer** (hash do color/depth buffer de uma cena de teste fixa, renderizada via WARP headless, comparado contra hash de baseline versionado como texto — nenhuma imagem gerada, armazenada ou comparada no CI)
- Pipeline `shader-validate.yml`: compila todos os shaders HLSL isoladamente, falha se algum não compilar
- Pipeline `nightly.yml`: build noturno com ASan/UBSan, static analysis (clang-tidy, cppcheck)
- Cache de dependências via `vcpkg` com binary caching no Actions (`actions/cache`, key por hash do `vcpkg.json`/lockfile)
- Cache de build/compilação (`ccache` ou `sccache` + cache do diretório de build do CMake/Ninja) para acelerar builds incrementais entre runs — invalida quando o compilador ou flags mudam
- Artefatos: builds compilados sobem como Release assets automaticamente em tags

---

## 2. ARQUITETURA EM CAMADAS

```
┌─────────────────────────────────────┐
│  Gameplay Layer (C# / KizuriNodes)   │
├─────────────────────────────────────┤
│  Game Systems (Quest, Dialogue, AI)  │
├─────────────────────────────────────┤
│  ECS Core (EnTT)                     │
├─────────────────────────────────────┤
│  World Streaming / Terrain / LOD     │
├─────────────────────────────────────┤
│  Renderer (Forward+ / Toon Pipeline) │
├─────────────────────────────────────┤
│  RHI (abstração sobre D3D11)         │
├─────────────────────────────────────┤
│  Core (Job System, Memory, Platform) │
└─────────────────────────────────────┘
```

### 2.1 Core
- Alocadores customizados: linear, pool, stack, free-list (zero `malloc` em hot path)
- Job System fiber-based (paralelismo real, não só thread pool ingênuo)
- Reflection system própria (para serialização + editor de propriedades automático)

### 2.2 RHI
- Interface abstrata (`IDevice`, `ICommandList`, `IPipelineState`) — D3D11 é a única implementação hoje, mas a interface já nasce pensando em D3D12/Vulkan
- Bindless-like resource management (mesmo em DX11, via texture arrays + heaps simulados)

### 2.3 Renderer
- Pipeline Forward+ com clustered lighting (necessário pra cenas abertas com muitas luzes dinâmicas — tochas, magia, clima)
- Toon shading customizado: ramp texture lighting, rim light, outline via inverted hull + normal-based silhouette
- Cascaded Shadow Maps (CSM) para sol direcional em área aberta
- Post-processing stack: Bloom, Color Grading (LUT), TAA, SSAO, Depth of Field
- GPU-driven culling: frustum + occlusion via Hi-Z, indirect draw calls

### 2.4 World/Streaming
- Divisão do mundo em **cells** (grid ou octree espacial)
- Streaming assíncrono via coroutines/job system: carrega células num raio configurável, descarrega fora dele
- Terrain: heightmap + texture splatting + virtual texturing para detalhe sem explodir VRAM — **desenhado/esculpido à mão pelo artista no Editor (Terrain Sculpting Tool), salvo como asset estático**; ruído procedural (FastNoise2) só entra como ponto de partida opcional, nunca como geração em runtime
- Sistema de "biomas" para variar vegetação/paleta por região — props posicionados via **ferramenta de placement assistido** (o artista pinta densidade/regras, mas cada instância final é curada e salva na cena, não recalculada em runtime)
- **Content Delivery (KizuriPatcher):** o streaming de mundo não assume que o asset já está em disco — antes de carregar uma célula, verifica o manifest local vs. remoto; se o pacote de dados daquela região ainda não foi baixado, dispara o download em background (ou bloqueia a entrada, dependendo da config do jogo) antes de materializar a célula. **Isso é opcional por projeto** — em "modo local", todos os assets já vêm empacotados junto com o `.exe` na build final (ideal pra jogos pequenos/médios, sem necessidade de servidor ou internet); o modo remoto só é ativado quando o projeto configura endpoints de CDN, sendo útil pra jogos grandes que não cabem num único download ou que precisam atualizar conteúdo sem republicar o executável

### 2.5 ECS
- EnTT como base, registry único por cena
- Sistemas: Transform, Render, Physics, Animation, AI, Audio — cada um opera em views (cache-friendly)

### 2.6 Física & Animação
- Jolt Physics: rigid bodies, character controller (`CharacterVirtual`) com **step offset** (auto-detecção de degraus até uma altura configurável — resolve escadas sem travar o personagem) + slope handling (limite de inclinação escalável)
- ozz-animation: skeletal animation, blending, IK de dois ossos (pés no terreno irregular)
- **Blend Shapes/Morph Targets (facial):** sistema próprio sobre a malha (ozz não cobre isso) — permite expressões faciais, sincronização labial e emoção por personagem, essencial pro nível de detalhe visual de personagens tipo Genshin

### 2.7 Gameplay/Scripting
- **C++ é exclusivo do core da engine** (renderer, física, streaming, ECS, job system) — nenhum código de jogo é escrito em C++, nunca
- **Todo jogo feito na Kizuri Engine é 100% C# ou KizuriNodes** — duas vias, mesmo runtime:
  - **C# (código):** hospedado via CoreCLR, compilado em runtime com Roslyn, hot-reload — pra quem prefere escrever código
  - **KizuriNodes (visual, por nós):** editor de script visual próprio (ImNodes), estilo Blueprints — pra quem não quer/sabe escrever código. Pipeline: **grafo de nós → C# (transpiler próprio) → IL (Roslyn, compilação em runtime) → nativo (JIT do CoreCLR)**. Sem VM de bytecode interpretado — o resultado final roda como código C# nativo, evitando o overhead clássico de dispatch nó-a-nó que o Blueprint da Unreal tem
- A engine expõe sua funcionalidade (ECS, física, animação, áudio) pro C# via uma **API gerenciada** (bindings gerados automaticamente sobre a camada de interop) — o desenvolvedor do jogo nunca vê C++, mesmo que o motor por baixo seja C++
- Bindings C++ ↔ C# via P/Invoke ou camada de interop customizada (structs blittable pra hot-path), mas essa camada é **invisível pro criador do jogo** — ele só enxerga a API C#
- **Skill/Ability Framework (KizuriAbility):** módulo C++ que fornece a *infraestrutura* de sincronização (animação + VFX Effekseer + hitbox/hurtbox + regra de dano/status + cooldown) — exposto ao dev via API C# gerada automaticamente. **O dev cria cada habilidade específica em C# (ou visualmente via KizuriNodes)**, referenciando um asset de dados (`.kability`) com os parâmetros daquela skill; o framework em C++ só garante que os sistemas disparam de forma sincronizada quando o C# chama `Ability.Execute()`

### 2.8 Ferramentas — Editor Visual + Project Hub
- **Estilização obrigatória:** Editor e Project Hub usam um **tema visual custom da Kizuri** (paleta de cores, tipografia, espaçamento e estilo de widgets do ImGui via `ImGuiStyle`/style editor) — nunca o visual padrão/cru do Dear ImGui ("cinza de fábrica"). Ícones Lucide (Seção 1.2) fazem parte desse tema, não um extra opcional
- **Project Hub** (app separado, leve): criar/abrir projetos, gerenciar versões da engine, templates de projeto, marketplace de plugins (futuro)
- **Editor:** ImGui docking multi-viewport, com:
  - Scene Hierarchy + Inspector (via reflection)
  - Asset Browser com thumbnails gerados
  - Gizmos de transformação (ImGuizmo)
  - Material Editor (node-based, via ImNodes)
  - KizuriNodes (editor de scripting visual — grafos que transpilam pra C#)
  - Terrain Sculpting tool
  - Timeline de animação
  - Profiler embutido próprio (KizuriProfiler, timeline CPU/GPU por frame)
  - Modo Play-in-Editor

---

## 3. ROADMAP POR FASES (Épicos → Sistemas → Marcos)

### FASE 0 — Fundação (Meses 1–3)
- [ ] Setup do repositório, CMake, vcpkg, estrutura de módulos
- [ ] CI: build.yml funcional (Windows Release/Debug)
- [ ] Platform layer: janela, input, timer
- [ ] Job System básico (fiber-based)
- [ ] Alocadores customizados + sistema de memória rastreável
- [ ] Logging (spdlog) + Profiling próprio (KizuriProfiler) integrados desde o dia 1

**Critério de aceite:**
- CI (`build.yml`) verde em Debug e Release no Windows, com MSVC, do zero (clone limpo)
- Executável abre uma janela de verdade, responde a teclado/mouse e fecha sem crash nem vazamento de memória (validado no MemoryTracker: uso volta a zero após shutdown)
- Job System roda um smoke test com jobs concorrentes em múltiplas threads sem deadlock, race ou job perdido (contagem de conclusão bate 100%)
- Todos os alocadores customizados têm testes unitários cobrindo alloc, free/reset, alinhamento e esgotamento de capacidade — suite passa no CI
- Log grava em arquivo e console simultaneamente; Profiler captura pelo menos um scope por frame e o dado é consultável (não só printado)
- Nenhuma chamada direta a `malloc`/`new` fora dos alocadores customizados no código do Core

### FASE 1 — RHI e Renderer Base (Meses 4–8)
- [ ] RHI abstrata sobre D3D11
- [ ] Pipeline de renderização de triângulo → mesh texturizada
- [ ] Sistema de shaders com hot-reload
- [ ] Câmera (free-cam + third-person)
- [ ] Forward+ renderer com clustered lighting
- [ ] CI: shader-validate.yml

**Critério de aceite:**
- Triângulo e depois uma mesh texturizada renderizam na tela passando só pela interface RHI abstrata (nenhuma chamada D3D11 fora de `KizuriRHI.lib`)
- Editar um `.hlsl` em disco reflete na cena rodando sem reiniciar o processo (hot-reload validado manualmente e/ou por teste automatizado)
- Câmera free-cam e third-person respondem a input sem input lag perceptível e sem clipping através de geometria de teste
- Cena de teste com múltiplas luzes dinâmicas simultâneas renderiza corretamente via clustered lighting, mantendo frame time dentro do budget definido na Seção 4
- `shader-validate.yml` roda no CI e falha de forma confiável quando um shader com erro de sintaxe é commitado (testado propositalmente)

### FASE 2 — Toon Rendering Pipeline (Meses 9–12)
- [ ] Ramp-based NPR lighting model
- [ ] Outline rendering (inverted hull)
- [ ] CSM para luz direcional
- [ ] Post-processing stack (Bloom, TAA, Color Grading)
- [ ] Testes de regressão visual via checksum de buffer (WARP headless, sem imagens no CI)

**Critério de aceite:**
- Ramp lighting toon visível e ajustável por parâmetro de material sem recompilar C++
- Outline via inverted hull renderiza sem artefatos grosseiros (z-fighting, gaps) nas meshes de teste padrão
- CSM não mostra light leaking ou peter-panning perceptível nas distâncias de cascata configuradas
- Cada efeito do post-process stack (Bloom, TAA, Color Grading) pode ser ligado/desligado individualmente sem quebrar o pipeline
- Testes de regressão visual via checksum de buffer rodam no CI (WARP headless) com baseline de hash versionada como texto no repo, e falham de forma confiável quando uma mudança visual não intencional é introduzida (testado propositalmente) — em nenhum momento uma imagem é gerada, salva ou comparada no pipeline

### FASE 3 — ECS e Reflection (Meses 13–15)
- [ ] Integração EnTT
- [ ] Sistema de reflection próprio (macros + type registry)
- [ ] Serialização de cena (flatbuffers)
- [ ] Editor: Inspector automático via reflection

**Critério de aceite:**
- Registry EnTT opera com pelo menos Transform + um sistema real iterando via view (não por ponteiro solto)
- Registrar um novo tipo via macro de reflection faz ele aparecer automaticamente no Inspector do editor, sem código manual de UI por tipo
- Cena salva em flatbuffers e recarregada reproduz 100% do estado original (teste de round-trip: salvar → carregar → comparar)

### FASE 4 — Mundo Aberto e Streaming (Meses 16–22)
- [ ] Sistema de células + streaming assíncrono
- [ ] Terrain Sculpting Tool no Editor + heightmap/splatting (autoria manual, salvo como asset estático)
- [ ] Virtual Texturing (tiled resources)
- [ ] Vegetation Placement Tool (assistido, com densidade pintável — não geração runtime)
- [ ] GPU-driven culling (Hi-Z occlusion)
- [ ] Marco: demo de "correr por 5km² sem hitch de streaming"

**Critério de aceite:**
- Demo jogável: correr 5km² continuamente, sem hitch de streaming perceptível (frame time sem spike acima do budget da Seção 4, medido pelo profiler, não estimado)
- Terrain Sculpting Tool esculpe, salva e recarrega um heightmap como asset estático, sem crash e sem perda de detalhe
- Virtual texturing carrega textura de detalhe sob demanda sem estourar o budget de VRAM definido para a cena de teste
- Vegetation placement tool gera instâncias determinísticas (mesma entrada → mesma saída) que ficam salvas na cena, não recalculadas a cada execução
- GPU-driven culling reduz draw calls de forma mensurável comparado a um baseline sem culling na mesma cena (número documentado, não estimado)

### FASE 5 — Física e Animação (Meses 23–28)
- [ ] Integração Jolt Physics
- [ ] Character controller (`CharacterVirtual`) com step offset (escadas) + slope handling (open-world: escalar, nadar, planar)
- [ ] Integração ozz-animation + blend trees
- [ ] IK de pés no terreno
- [ ] Sistema de Blend Shapes/Morph Targets (facial, sync labial, emoção)
- [ ] Integração Effekseer (KizuriVFX): partículas, trails, efeitos de shader
- [ ] Editor: Timeline de animação + preview de VFX

**Critério de aceite:**
- Character controller sobe degraus até o step offset configurado sem travar e não sobe degraus acima do limite
- Character controller respeita o slope limit configurado (não escala rampas acima do ângulo definido)
- Blend tree faz transição visualmente suave (sem popping) entre no mínimo duas animações num personagem de teste
- IK de pés adapta corretamente a pelo menos um trecho de terreno irregular de teste, sem penetração visível no chão
- Blend shapes aplicam expressão facial visível e sincronização labial básica num personagem de teste
- Efeito Effekseer dispara e renderiza corretamente integrado ao pipeline (não como janela/preview isolado)

### FASE 6 — Ferramentas (Meses 29–34)
- [ ] Project Hub standalone
- [ ] Editor: Asset Browser, Material Editor node-based
- [ ] Terrain Sculpting Tool
- [ ] Play-in-Editor mode
- [ ] CI: nightly.yml com ASan/UBSan + static analysis

**Critério de aceite:**
- Project Hub cria e abre um projeto do zero sem exigir edição manual de arquivo nenhum
- Material Editor gera um `.kmat` válido cujo resultado visual no jogo bate com o preview mostrado no editor
- Play-in-Editor entra e sai do modo de jogo sem corromper ou alterar permanentemente o estado da cena salva
- `nightly.yml` roda ASan/UBSan + static analysis e falha de forma confiável quando um erro de memória/UB é injetado propositalmente num teste

### FASE 7 — Gameplay Framework (Meses 35–40)
- [ ] Hosting CoreCLR embutido no engine (.NET 10, LTS, runtime dentro do processo C++)
- [ ] Pipeline de compilação em runtime via Roslyn (hot-reload de scripts C#)
- [ ] Camada de interop C++ ↔ C# (structs blittable, marshalling mínimo em hot-path)
- [ ] Geração automática de bindings C# sobre a API do core C++ (via reflection/codegen — nenhum jogo toca C++ diretamente)
- [ ] KizuriNodes: editor visual de scripting (ImNodes) com transpilação de grafo → C#
- [ ] Biblioteca de nós padrão (eventos, condições, ações de gameplay, quests, diálogo)
- [ ] Sistema de Quests + Diálogo (data-driven, scriptável em C# ou KizuriNodes)
- [ ] AI: behavior trees ou utility AI para NPCs
- [ ] KizuriAbility: infraestrutura C++ do Skill/Ability Framework (sincronização animação+VFX+hitbox+dano/status+cooldown) + API C# gerada sobre ela
- [ ] Editor: Ability Editor (dev monta e testa cada skill específica via inspector + C#/KizuriNodes, sem tocar C++)
- [ ] Sistema de inventário/progressão (reusável)

**Critério de aceite:**
- CoreCLR hospedado no processo executa um script C# de teste e o resultado é observável (log, efeito na cena)
- Hot-reload de C# aplica uma mudança de código sem reiniciar o editor nem perder estado da sessão
- Um grafo simples no KizuriNodes (ex: mover personagem ao apertar tecla) transpila para C# válido e executa com o mesmo resultado que o código escrito à mão faria
- Uma skill de teste no KizuriAbility dispara animação, VFX, hitbox e dano de forma sincronizada dentro de uma janela aceitável de frames (sem dessincronia visível)
- Um fluxo de quest/diálogo data-driven completa ponta a ponta (início → objetivo → conclusão) sem código C++ envolvido, só dados + C#/KizuriNodes

### FASE 8 — Polish e Otimização Final (Meses 41–48)
- [ ] KizuriPatcher: sistema de manifest + download delta + verificação de integridade (content delivery remoto)
- [ ] Passo completo de otimização (profiling real, budgets de frame por sistema)
- [ ] LOD automático + impostors para vegetação distante
- [ ] Áudio 3D posicional + música dinâmica por bioma
- [ ] Sistema de save/load robusto
- [ ] Documentação completa (site com Doxygen ou mdBook)

**Critério de aceite:**
- KizuriPatcher baixa e aplica um patch de teste, detectando corretamente um pacote corrompido quando testado propositalmente (checagem de integridade falha como esperado)
- Todos os budgets de frame definidos na Seção 4 são cumpridos numa cena de referência, medido pelo profiler (não estimado)
- LOD automático e impostors reduzem triangle count de forma mensurável à distância, sem pop visual perceptível na transição
- Save/load preserva 100% do estado relevante do jogador numa cena de teste (teste de round-trip)
- Site de documentação publicado, navegável, sem links quebrados

---

## 4. ESTRATÉGIA DE OTIMIZAÇÃO (transversal, não é uma fase — é regra desde o dia 1)

- **Budget de frame:** definir orçamento por sistema (ex: 2ms culling, 4ms shadows, 3ms streaming) e validar no profiler a cada milestone
- **Data-Oriented sempre:** nenhum sistema hot-path deve iterar ponteiros dispersos na heap
- **GPU-driven rendering:** reduzir draw calls via indirect drawing e instancing agressivo
- **Streaming preditivo:** carregar células com base na velocidade/direção do jogador, não só distância
- **Zero alocação no game loop:** todo alloc crítico vem de pools pré-alocados
- **GC do C# sob controle:** lógica de gameplay em C# não deve tocar sistemas hot-path (renderer, física, streaming) diretamente — apenas orquestrar via chamadas pro core C++; usar `Server GC` do .NET e evitar alocações por frame em scripts pra não gerar GC pauses perceptíveis

---

## 5. RISCOS (projeto solo, honestidade arquitetural)

| Risco | Mitigação |
|---|---|
| Escopo throwing (feature creep) | Roadmap fechado por fase, sem pular fases |
| Burnout (anos sozinho) | Marcos pequenos e visíveis (demos jogáveis a cada fase) |
| Ferramentas de terceiros mudarem/quebrarem | Pin de versões via vcpkg lockfile |
| DX11 ficar datado | RHI abstrata permite migrar para DX12/Vulkan sem reescrever o resto |
| Falta de conteúdo artístico (você não é 100 artistas) | Usar assets de placeholder/marketplace nas fases iniciais, focar em tech, não em conteúdo final |

---

## 6. CONVENÇÕES DE CÓDIGO (sem comentários)

- **Nenhum stub, exemplo ou placeholder.** Todo módulo/arquivo criado deve ser funcional de verdade dentro do escopo da fase atual — se um sistema ainda não deve existir (ex: Editor antes da Fase 6), ele simplesmente não é criado, em vez de existir como esqueleto vazio "pra não quebrar o build"
- Nomes de função/variável extremamente descritivos (`ComputeClusteredLightIndices` não `CalcLights`)
- Funções pequenas (uma responsabilidade) — se precisa de comentário pra explicar, é sinal de que devia ser quebrada
- Tipos fortes em vez de primitivos soltos (`EntityId` em vez de `uint32_t` cru)
- Testes unitários (Catch2) documentam comportamento no lugar de comentários
- Documentação externa em `/docs` (mdBook), nunca inline no `.cpp`/`.h`

---

## 7. ESTRUTURA MODULAR (Build)

**Um `.exe`, várias `.lib`** — cada camada/sistema da engine é uma biblioteca estática separada, todas linkadas no executável final:

```
KizuriEngine.exe  (linka tudo abaixo)
│
├── KizuriCore.lib          (platform, memória, job system, logging)
├── KizuriRHI.lib           (abstração DX11)
├── KizuriRenderer.lib      (pipeline forward+, toon shading, post-process)
├── KizuriECS.lib           (EnTT wrapper, reflection)
├── KizuriWorld.lib         (streaming, terrain, cells)
├── KizuriPhysics.lib       (wrapper sobre Jolt)
├── KizuriAnimation.lib     (wrapper sobre ozz)
├── KizuriScripting.lib     (hosting CoreCLR + interop C#)
├── KizuriAudio.lib         (wrapper sobre miniaudio)
├── KizuriVFX.lib           (wrapper sobre Effekseer, partículas, trails, efeitos de shader especiais)
├── KizuriAbility.lib       (Skill/Ability Framework: dispara animação+VFX+hitbox+efeito de gameplay sincronizados)
├── KizuriPatcher.lib       (content delivery: manifest, download, delta patching, verificação de integridade)
└── KizuriEditor.lib        (ImGui + ferramentas — só linkada em builds de editor)
```

- Cada `.lib` é um projeto CMake independente, com headers públicos próprios e testes (Catch2) isolados
- `KizuriEditor.lib` entra condicionalmente via flag de build (`KIZURI_WITH_EDITOR=ON`) — o executável final do jogo (Runtime) não carrega peso de ferramentas de editor
- Dependências entre libs declaradas explicitamente via `target_link_libraries` no CMake, sem acoplamento circular — isso reforça a arquitetura em camadas da Seção 2
- CI builda cada `.lib` isoladamente antes de linkar o `.exe` final, pra pegar quebras de módulo cedo

---

## 8. SISTEMA DE SHADERS (Pipeline fixo + Materiais plugáveis)

**Princípio:** o *pipeline* de renderização é fixo (definido pela engine); o *shading de superfície* é plugável por projeto/material.

### 8.1 Camada fixa (engine, não muda por projeto)
- Passes fixos: Depth Prepass → Shadow Pass (CSM) → Forward+ Lighting Pass → Post-Process Stack
- Cada pass define um "contrato" de entrada/saída (ex: o Lighting Pass espera receber albedo, normal, roughness-like params do material)
- Isso garante que qualquer material novo funciona automaticamente com culling, sombras, clustered lighting, sem reescrever infraestrutura

### 8.2 Camada plugável (por projeto, via Material System)
- **Shaders-base fornecidos pela engine:**
  - `ToonNPR.hlsl` — ramp lighting + rim light + suporte a outline (o principal pro gênero Genshin-like)
  - `PBRStandard.hlsl` — pra props/ambientes que não precisam de toon
  - `Unlit.hlsl` — UI, efeitos, VFX simples
- **Material Asset (`.kmat`):** arquivo data-driven que referencia um shader-base + valores de parâmetros (texturas, cores, floats) — sem recompilar C++ nem o shader em si
- **Extensão via Shader Graph (Editor, Fase 6):** artistas/designers montam variações visuais (node-based, ImNodes) que geram HLSL por baixo, sem escrever código
- **Extensão via HLSL direto (avançado):** projeto pode escrever um shader customizado do zero, desde que respeite o contrato de I/O dos passes fixos

### 8.3 Hot-reload
- File watcher (efsw) detecta mudança em `.hlsl` ou `.kmat` e recompila/reaplica em runtime, sem restart do editor — crítico pra iteração rápida de arte

---

## 9. PACKAGING E DISTRIBUIÇÃO DO JOGO

**Modelo:** um executável nativo que hospeda o runtime C# — não é tudo compilado junto num binário monolítico.

- **`KizuriRuntime.exe`** — binário nativo C++, linka estaticamente todas as `.lib` do core (Renderer, Physics, ECS, VFX, Ability, etc.) e inicializa o CoreCLR embutido no processo
- **`Game.dll`** — assembly .NET com todo o código C# do jogo (lógica escrita pelo dev, incluindo o que veio do KizuriNodes transpilado), carregado pelo `.exe` na inicialização
- **Assets** (texturas, `.kmat`, `.kability`, cenas) — dependendo da configuração do projeto: **modo local** (empacotados junto no `.exe`/pasta de instalação, jogo autocontido, sem internet necessária) ou **modo remoto** (entregues via streaming/KizuriPatcher sob demanda, útil pra jogos grandes que não cabem num único instalador ou que precisam de updates de conteúdo sem republicar o executável)

### 9.1 Build de Desenvolvimento vs. Shipping
| | Dev/Editor | Shipping (build final) |
|---|---|---|
| Compilação do C# | JIT em runtime via Roslyn | **Native AOT** (compilado pra código nativo antes do lançamento) |
| Hot-reload | Sim | Não (desnecessário em produto final) |
| Dependência de .NET runtime instalado no PC do jogador | Sim (embutido no editor) | **Não** — Native AOT elimina essa dependência |
| Performance de startup | JIT warmup na primeira execução | Sem warmup — já nasce nativo |

Usar Native AOT no build final remove o overhead de JIT na inicialização e a necessidade do jogador ter o .NET instalado — o `Game.dll` vira código de máquina nativo antes mesmo de sair da sua máquina de build.

---

### 9.2 Fluxo de Exportação (Editor → Build Final)

**1. Configuração (painel de Export no Editor)**
- Build Config: Debug ou Release
- Modo de distribuição: Local ou Remoto
- Plataforma: Windows

**2. Pipeline de build (headless, roda sem UI)**
- Compilação do `Game.dll`: Native AOT (Release) ou JIT (Debug)
- **Cook de assets:** conversão dos formatos de edição pros formatos de runtime (texturas → BC7, cenas → binário flatbuffers), descartando metadata/thumbnails que só servem pro Editor
- **Asset stripping:** percorre o grafo de dependências a partir das cenas usadas e remove tudo que não é referenciado, evitando build inflada
- **Empacotamento:** modo Local copia tudo junto do `.exe`; modo Remoto gera o manifest do KizuriPatcher e separa os assets em chunks pra CDN
- Cópia do `KizuriRuntime.exe` pré-compilado pra pasta final

**3. Saída:** pasta pronta pra instalador (Inno Setup) ou upload direto (Steam, itch.io)

**Integração com CI:** o pipeline de export roda em modo headless/batch, então o GitHub Actions pode gerar builds completas do jogo (não só da engine) automaticamente a cada tag/release.

### 9.3 Splash Screen (Branding de Inicialização)

- Tela fixa exibida na abertura do `.exe`, antes do carregamento do jogo em si (RHI já ativo, mas antes de ECS/streaming/scripting subirem)
- **O logo da Kizuri Engine é obrigatório e não configurável** — todo jogo exportado pela engine exibe o splash da Kizuri, sem opção de remover (mesmo padrão de Unreal/Unity com suas engines)
- O dev pode **adicionar** o logo do próprio estúdio em sequência (antes ou depois do splash da engine), mas não pode **remover** o da Kizuri
- Assets de splash do estúdio (imagem/duração) definidos na configuração de Export (Seção 9.2); o splash da Kizuri já vem embutido no `KizuriRuntime.exe`

---

*Documento vivo — atualizar a cada marco de fase concluído.*
