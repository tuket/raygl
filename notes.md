Reflection
---

$$
f_s(\vec{i}, \vec{o}, \vec{m}) = f_r(\vec{i}, \vec{o}, \vec{m}) + f_t(\vec{i}, \vec{o}, \vec{m})
$$

We are going to make the following assumption for our ray tracer: a surface can be either transparent or diffuse but not both at the same time. This is because diffuse light is a spacial case of transmision for very high density participative media.

$f_r$: reflection <br>
$f_t$: transmision (or refraction)<br>

Reflection
---

$$
f_r(\vec{i}, \vec{o}, \vec{m}) =
\frac{F(\vec{i}, \vec{h_r}) G(\vec{i}, \vec{o}, \vec{h_r}) D(\vec{h_r})}
{4 |\vec{i} \cdot \vec{n}| |\vec{o} \cdot \vec{n}|}
$$

Transmision
---

$$
f_t(\vec{i}, \vec{o}, \vec{m}) =
\frac{|\vec{i} \cdot \vec{h_t}| |\vec{o} \cdot \vec{h_t}|}
{|\vec{i} \cdot \vec{n}| |\vec{o} \cdot \vec{n}|}
\frac{\eta_o^2 (1-F(\vec{i}, \vec{h_t})) G(\vec{i}, \vec{o}, \vec{h_t}) D{\vec{h_t}}}
{(\eta_i (\vec{i} \cdot \vec{h_t}) + \eta_o (\vec{o} \cdot \vec{h_t}))^2}
$$

F: Exact Fresnel
---