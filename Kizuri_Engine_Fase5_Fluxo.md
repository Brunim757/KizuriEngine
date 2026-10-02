# Fase 5 — Iluminação avançada e sombras: Fluxo Detalhado

**Baseado em:** `Kizuri_Engine_TDD.md` e `Kizuri_Engine_Roadmap.md`, Fase 5.
**Objetivo deste documento:** deixar explícito, passo a passo, como cada parte da iluminação funciona — pra você e pra qualquer agente de IA executando a fase não terem dúvida de comportamento.

**Decisões registradas nesta versão** (fechadas na primeira passagem de implementação, revertida antes da validação final — valem pra reimplementação):
- Entidade nasce só com `Transform`; `Mesh` e `Light` entram via Add Component ou drag-and-drop, nunca pré-anexados.
- Suavização de sombra é **PCSS**, não PCF (troca decidida pelo autor; PCF foi removido, não mantido como opção).
- Céu segue o Roadmap à risca: gradiente + sol, **reconstruído via depth + matriz inversa de view-projection** (a primeira passagem usou construção direta de raios — não repetir).
- FSR1 usa os arquivos de referência da AMD vendorados (`third_party/fsr1`, licença MIT preservada) e o deploy **precisa** copiá-los junto com `Shaders/` (sem isso o editor falha no init).
- Luz de hemisphere foi **proposta e revertida** — pendente de decisão do autor (ver Seção 9).
- Tilt de câmera reportado durante a 5B segue **sem diagnóstico** — método de bissecção registrado na Seção 9.

---

## 1. Princípio geral

A cena tem **um sol** (direcional, sempre ativo) + até **16 luzes pontuais/spot** dinâmicas simultâneas. O sol tem sombra CSM; point/spot têm sombra só se marcadas com Cast Shadow, dentro de um orçamento fixo de mapas (Seção 4). Luz além do orçamento ilumina normalmente, sem sombra — nunca quebra, nunca some.

**Regra fixa:** difuso/specular seguem PBR Cook-Torrance (GGX + Schlick, mesma paridade CPU/GPU da Fase 2). Nada de "wrap" no difuso nem atalho visual que quebre a paridade.

---

## 2. Componente Light + Sol (5A)

```
1. Dev cria entidade (nasce só com Transform)
2. Inspector → Add Component → Light (ou arrasta entidade pronta)
3. Light nasce como Point (branca, intensidade 3, raio 10); Type alterna Point/Spot
4. Spot usa a direção da entidade (frente do transform) + ângulo do cone
5. Sem nada selecionado, o Inspector mostra o Sol da cena (direção, cor,
   intensidade) — não é componente de entidade, é estado da cena
6. Tudo com Undo (adicionar, remover, editar tipo/cor/intensidade/raio/ângulo)
   e tudo persiste no .kzscene
```

**PBR point/spot:** mesma atenuação por distância da Fase 2 (`(1-(d/r)²)²`), cone do spot com `smoothstep` no cosseno do meio-ângulo. Espelho CPU/GPU testado no CI (dentro do range ilumina, fora zera; fora do cone zera).

**Serialização:** linha `SUN ...` (direção, cor, intensidade, flags de sombra, softness) e linha `LIGHT ...` (tipo, cor, intensidade, raio, ângulo, castShadow, softness) — só gravadas quando o componente existe. Campos novos são sempre opcionais na leitura (arquivo antigo carrega com padrões).

---

## 3. CSM do sol (5B)

```
1. A cada frame, do frustum da câmera extraem-se os splits (2 a 4 cascatas,
   configurável; mistura log/uniforme com lambda 0.5)
2. Pra cada cascata: cantos do sub-frustum → centroide → matriz ortográfica
   do sol com snap em texel (sem shimmer ao mover a câmera)
3. Cena é desenhada uma vez por cascata num atlas 2048² (só profundidade)
4. No lighting, cada pixel escolhe a cascata pela profundidade de vista
   e compara via PCSS (Seção 5)
5. CSM é dinâmico: tudo recomputado todo frame a partir da câmera
```

**Debug de cascatas:** toggle no Viewport pinta a cena com uma cor por cascata (vermelho/verde/azul/amarelo). Cascata como bloco diagonal torto = matriz errada, visível na hora.

---

## 4. Orçamento de mapas de sombra (5B)

Atlas único de 2048² dividido em **4 tiles de 1024²** (sem bindless no DX11):

```
1. Sol ocupa os primeiros N tiles (N = número de cascatas, 2 a 4)
2. Spots com Cast Shadow (no máximo as mais próximas) ocupam os tiles
   restantes, 1 tile cada (máximo 2 spots com sombra por frame)
3. Point com Cast Shadow: só a mais próxima, num cube map dedicado de 512²
   (6 faces, profundidade linearizada na amostragem)
4. Limite total considerado: 4 luzes com sombra (as mais próximas da câmera);
   excedentes iluminam sem sombra, sem aviso e sem erro
```

**RHI necessário:** formato de profundidade legível no shader (`R32_DEPTH`), cube map de sombra com DSV por face, sampler de borda branca, bias de inclinação no rasterizador. Backend Null implementa tudo como no-op (sombras desligadas com elegância, sem stub).

---

## 5. PCSS no lugar do PCF (5B, decisão do autor)

PCF 3×3 foi removido; no lugar, PCSS completo (contact hardening de verdade):

```
1. Busca de bloqueadores: 16 taps Poisson num raio proporcional ao tamanho
   da fonte de luz; sem bloqueador → totalmente iluminado
2. Penumbra: (profundidade_recebedor − profundidade_bloqueador) × tamanho /
   profundidade_bloqueador (física de triângulos semelhantes)
3. Filtro: 16 taps Poisson com raio = penumbra projetada em UV, clampado
   pra não vazar entre tiles do atlas
4. Tamanho da fonte 0.0 = sombra dura (1 tap, caminho barato)
```

**Softness (slider 0.0–2.0):** no sol e em cada luz com Cast Shadow (padrão sol 0.5, luzes 0.3). Some com Undo, persiste no `.kzscene`.

**Anti-acne:** normalBias 0.03 no shader + slope bias 2.0 no rasterizador do depth pass.

---

## 6. Céu procedural + grade (5C)

```
1. Céu atrelado ao sol (gradiente + disco solar + halo), conforme o Roadmap:
   reconstrução do raio de visão via depth + matriz inversa de view-projection
2. Tonemapping ACES (aproximação de Narkowicz) + exposição (multiplicador,
   padrão 1.0) aplicados no fim do lighting, inclusive no céu
3. Toggles de viewport (estado de visualização, não serializados): ACES, Sky,
   Exposure, Bloom
```

**Bloom LDR seletivo (simples):** threshold 0.8 em meia resolução → blur gaussiano separável → soma com força ajustável (0 = desligado). Cadeia: cena → bloom → upscale → sharpen → FXAA.

---

## 7. FSR1 + instancing + culling + FXAA (5C)

**FSR1 (upscaling espacial):**
```
1. Render Scale 0.5–1.0 no Viewport (padrão 1.0 = bypass, custo zero)
2. Cena (incluindo bloom) renderiza na resolução reduzida
3. EASU reconstrói pra resolução cheia → RCAS aplica nitidez (slider Sharp)
4. Constantes EASU/RCAS calculadas na CPU via ffx_fsr1.h; shaders wrappers
   incluem os headers vendorados
5. Deploy copia third_party/fsr1 junto com Shaders/ (lição aprendida:
   sem isso, include relativo quebra e o editor não inicializa)
```

**Instancing agressivo:** draws com mesmo mesh e material são agrupados por frame num draw instanciado (matrizes de mundo em buffer de instância). Vale pro depth pass das sombras também. Não é otimização futura — é o caminho padrão de desenho.

**Culling por frustum (CPU):** cada objeto é testado (esfera da AABB do mesh) antes do desenho principal; fora do frustum não desenha. Casters de sombra fora da câmera **continuam** indo pro depth pass (sombra precisa deles).

**FXAA:** último passo da cadeia, sobre a imagem final LDR (depois do RCAS).

**Ordem final da cadeia:** geometria → CSM/sombras → lighting (céu+ACES) → bloom → EASU → RCAS → FXAA → tela.

---

## 8. Guardiões de CI (o que valida sem GPU)

- **ShaderCompile:** compila todos os HLSL com warnings-as-errors — em **todos** os layouts onde o arquivo existir (raiz `Shaders/` **e** `build/bin/Release/Shaders/`, que é o layout instalado). Foi esse teste que pegou includes quebrados e erro X3570 antes de chegarem ao PC do autor.
- **Espelhos CPU:** splits de cascata, matrizes sun/spot/point (cobertura do frustum), alocação de tiles, fórmula de penumbra, ACES, gradiente do céu — mesma matemática do shader, testada no `KizuriHello`.
- Limites honestos sem GPU no CI: qualidade visual de sombra/bloom/FSR/FXAA só valida no olho, no editor.

---

## 9. Pontos em aberto (não são parte do aceite, mas registrados)

**Tilt de câmera (reportado na 5B, sem diagnóstico):** horizonte descrito como "ficando vertical" ao mover. Eliminado por leitura de código: sem matriz inversa no pipeline, base do LookAt sem roll por construção, normais OK nos dois loaders, aspecto consistente. Bisseção sem mexer em código: desligar `Shadows` do sol — se sumir, o culpado é o caminho de sombra da 5B (e o Debug CSM mostra onde); se persistir, o próximo corte é Sky off, depois ACES off.

**Luz de hemisphere (proposta, revertida):** com uma única luz, faces opostas ao sol iam a ~3% (quase preto) + ACES. Proposta era céu/chão/intensidade no sol, com Undo e serialização. Autor pediu revert — **não reimplementar sem decisão explícita nova.**

---

## 10. "Pronto quando" da Fase 5 (critério de aceite, do Roadmap, adaptado)

- Uma cena com 10+ luzes dinâmicas e sombras suaves (PCSS) roda sem queda perceptível de frame.
- Uma luz (Point/Spot) pode ser criada e ajustada inteiramente pelo Inspector via Add Component; o sol, sem nada selecionado.
- Cascatas configuráveis (2–4) com debug visual; excedente de luzes com sombra degrada pra "sem sombra", nunca quebra.
- Céu acompanha o sol; ACES/exposição/bloom/FSR/FXAA ajustáveis no Viewport; cadeia completa sem erro de init no layout instalado.
- Arquivo `.kzscene` antigo (sem os campos novos) abre com padrões sãos.
