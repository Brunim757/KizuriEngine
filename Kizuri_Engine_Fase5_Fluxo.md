# Fase 5 — Iluminação avançada e sombras: Fluxo Detalhado

**Baseado em:** `Kizuri_Engine_TDD.md` e `Kizuri_Engine_Roadmap.md`, Fase 5.
**Objetivo deste documento:** deixar explícito, passo a passo, como cada parte da iluminação funciona — pra você e pra qualquer agente de IA executando a fase não terem dúvida de comportamento.

**Decisões fechadas:** entidade nasce só com `Transform` (Mesh/Light via Add Component ou drag-and-drop); suavização é **PCSS** (PCF removido); céu via depth + matriz inversa (como manda o Roadmap); FSR1 vendorado com cópia no deploy. **Em aberto:** hemisphere (proposto, revertido) e tilt de câmera (Seção 9).

---

## 1. Princípio geral

Um sol direcional (sempre ativo) + até 16 luzes point/spot dinâmicas. Sol tem CSM; point/spot só têm sombra com Cast Shadow, dentro de orçamento fixo (Seção 4). Luz além do orçamento ilumina sem sombra — nunca quebra.

**Regra fixa:** PBR Cook-Torrance com paridade CPU/GPU. Sem atalho que quebre a paridade.

---

## 2. Light + Sol (5A)

```
1. Entidade nasce só com Transform
2. Inspector → Add Component → Light (nasce Point; Type alterna Point/Spot)
3. Spot usa a frente do transform + ângulo do cone
4. Sem seleção, o Inspector mostra o Sol da cena (direção, cor, intensidade)
5. Tudo com Undo e tudo persiste no .kzscene
```

Point/spot usam a atenuação da Fase 2; cone com smoothstep no meio-ângulo. `SUN`/`LIGHT` só gravam com componente presente; campo novo é sempre opcional na leitura.

---

## 3. CSM do sol (5B)

```
1. Splits do frustum da câmera (2 a 4 cascatas, mistura log/uniforme)
2. Uma matriz ortográfica por cascata, com snap em texel
3. Cena desenhada uma vez por cascata no atlas (só profundidade)
4. Pixel escolhe a cascata pela profundidade de vista
5. Tudo recomputado todo frame (CSM dinâmico)
```

Debug no Viewport pinta uma cor por cascata; bloco torto = matriz errada.

---

## 4. Orçamento de sombras (5B)

Atlas 2048² em 4 tiles de 1024². Sol ocupa os N primeiros (N = cascatas); spots com sombra pegam o resto (máx. 2); point com sombra é só o mais próximo, num cube 512². Limite considerado: 4 luzes (as mais próximas); resto ilumina sem sombra.

RHI: profundidade legível no shader, cube com DSV por face, sampler de borda branca, slope bias. Null vira no-op.

---

## 5. PCSS (5B)

```
1. 16 taps Poisson procuram bloqueadores; sem bloqueador → iluminado
2. Penumbra = (recebedor − bloqueador) × tamanho / bloqueador
3. 16 taps com raio da penumbra, clampado pra não vazar de tile
4. Softness 0.0 = sombra dura (1 tap); padrão sol 0.5, luzes 0.3
```

Anti-acne: normalBias 0.03 + slope bias 2.0.

---

## 6. Céu + grade (5C)

Céu atrelado ao sol (gradiente + disco), raio reconstruído via depth + matriz inversa de view-projection. ACES + exposição no fim do lighting, inclusive no céu. Toggles de viewport (não serializam): ACES, Sky, Exposure, Bloom.

Bloom LDR: threshold 0.8 em meia resolução → blur separável → soma (0 = off).

---

## 7. FSR1 + instancing + culling + FXAA (5C)

```
1. Render Scale 0.5–1.0 (1.0 = bypass); cena renderiza reduzida
2. EASU reconstrói pra cheia → RCAS afia (Sharp); constantes via ffx_fsr1.h
3. Deploy copia third_party/fsr1 junto com Shaders/ (sem isso, init falha)
```

Instancing é o caminho padrão (vale pro depth pass). Culling de frustum na CPU só no desenho principal — caster de sombra fora da câmera continua indo ao depth pass. FXAA por último, após o RCAS.

Cadeia: geometria → sombras → lighting → bloom → EASU → RCAS → FXAA → tela.

---

## 8. Guardiões de CI

- **ShaderCompile:** todos os HLSL com warnings-as-errors, na raiz `Shaders/` **e** em `build/bin/Release/Shaders/` (layout instalado).
- **Espelhos CPU:** splits, matrizes, tiles, penumbra, ACES, céu — testados no `KizuriHello`. Qualidade visual só no olho.

---

## 9. Em aberto

**Tilt de câmera (5B, sem diagnóstico):** horizonte descrito como vertical ao mover. Eliminado por leitura: sem inversa no código, base sem roll, normais e aspecto OK. Bisseção sem código: sol com `Shadows` off — sumiu, é sombra (Debug CSM mostra onde); persistiu, é luz/céu (testar Sky off, ACES off).

**Hemisphere (revertido):** faces opostas ao sol iam a ~3%. Proposta (céu/chão/intensidade no sol) revertida a pedido — não reimplementar sem decisão nova.

---

## 10. "Pronto quando" da Fase 5

- 10+ luzes dinâmicas com PCSS sem queda perceptível de frame.
- Luz criada/ajustada pelo Inspector; sol sem nada selecionado.
- Cascatas 2–4 com debug; excedente degrada pra sem sombra.
- Céu segue o sol; ACES/bloom/FSR/FXAA no Viewport; init OK no layout instalado.
- `.kzscene` antigo abre com padrões.
