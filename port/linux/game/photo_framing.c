/* Numeric framing files shared by photo mode and asset-free native probes. */
#include "photo_framing.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float photo_fov_step(float fov, int steps)
{
	float value = fov + steps * 0.02f;
	return fminf(2.6f, fmaxf(0.2f, value));
}

void photo_level_up(const float forward[3], float up[3])
{
	float horizontal = hypotf(forward[0], forward[1]);
	if (horizontal < 1e-6f)
	{
		/* At a pole, any horizontal up axis is level; choose a stable one. */
		up[0] = 1; up[1] = 0; up[2] = 0;
	}
	else
	{
		float length = hypotf(horizontal, forward[2]);
		up[0] = -forward[0] * forward[2] / (horizontal * length);
		up[1] = -forward[1] * forward[2] / (horizontal * length);
		up[2] = horizontal / length;
	}
}

/* flying_camera_update rotates the level up axis about forward with rotate_vector_about_axis:
up cos(roll) + (forward x up) sin(roll). */
void photo_roll_up(const float forward[3], float roll, float up[3])
{
	float level[3], side[3], c = cosf(roll), s = sinf(roll);
	int i;
	photo_level_up(forward, level);
	side[0] = forward[1] * level[2] - forward[2] * level[1];
	side[1] = forward[2] * level[0] - forward[0] * level[2];
	side[2] = forward[0] * level[1] - forward[1] * level[0];
	for (i = 0; i < 3; i++) up[i] = level[i] * c + side[i] * s;
}

float photo_roll_from_up(const float forward[3], const float up[3])
{
	float level[3], side[3];
	photo_level_up(forward, level);
	side[0] = forward[1] * level[2] - forward[2] * level[1];
	side[1] = forward[2] * level[0] - forward[0] * level[2];
	side[2] = forward[0] * level[1] - forward[1] * level[0];
	return atan2f(up[0] * side[0] + up[1] * side[1] + up[2] * side[2],
		up[0] * level[0] + up[1] * level[1] + up[2] * level[2]);
}

float photo_roll_step(float roll, int direction, float seconds, int speed_step)
{
	const float pi = 3.14159265f;
	if (direction && seconds > 0.f) roll += (float)direction * PHOTO_ROLL_SPEED * powf(2.f, (float)speed_step) * seconds;
	while (roll > pi) roll -= 2.f * pi;
	while (roll < -pi) roll += 2.f * pi;
	return roll;
}

int photo_map_key(const char *scenario, int custom, const unsigned char *hash, char *key, size_t size)
{
	const char *name = scenario, *p;
	size_t n;
	if (!scenario || !size) return 0;
	if (custom)
	{
		if (!hash || size < 73) return 0;
		strcpy(key, "blake2b:");
		for (n = 0; n < 32; n++) sprintf(key + 8 + n * 2, "%02x", hash[n]);
		return 1;
	}
	for (p = scenario; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
	n = strlen(name);
	if (!n || n + 7 > size) return 0;
	strcpy(key, "stock:");
	for (p = name; *p; p++)
	{
		unsigned char ch = (unsigned char)tolower((unsigned char)*p);
		if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) return 0;
		key[6 + p - name] = ch;
	}
	key[6 + n] = 0;
	return 1;
}

int photo_stock_scenario(const char *name)
{
	static const char *stock[] = {
		"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
		"beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation",
		"hangemhigh", "longest", "prisoner", "putput", "ratrace", "sidewinder", "wizard"
	};
	char key[256];
	size_t i;
	if (!photo_map_key(name,0,NULL,key,sizeof(key))) return 0;
	for (i=0;i<sizeof(stock)/sizeof(stock[0]);i++) if (!strcmp(key+6,stock[i])) return 1;
	return 0;
}

static int key_valid(const char *key)
{
	char derived[256];
	size_t i;
	if (!strncmp(key, "stock:", 6))
		return photo_map_key(key + 6, 0, NULL, derived, sizeof(derived)) && !strcmp(key, derived);
	if (strncmp(key, "blake2b:", 8) || strlen(key) != 72) return 0;
	for (i = 8; i < 72; i++) if (!strchr("0123456789abcdefABCDEF", key[i])) return 0;
	return 1;
}

static int vector_valid(const float *v, int n, int bounded)
{
	int i;
	for (i = 0; i < n; i++) if (!isfinite(v[i]) || (bounded && (v[i] < 0 || v[i] > 1))) return 0;
	return 1;
}

int photo_shot_valid(const struct photo_shot *s)
{
	float f = 0, u = 0, dot = 0;
	int i;
	if (!vector_valid(s->position, 3, 0) || !vector_valid(s->forward, 3, 0) ||
		!vector_valid(s->up, 3, 0) || !vector_valid(s->focus, 2, 1) || !vector_valid(s->safe, 4, 1)) return 0;
	for (i = 0; i < 3; i++) { f += s->forward[i]*s->forward[i]; u += s->up[i]*s->up[i]; dot += s->forward[i]*s->up[i]; }
	return fabsf(sqrtf(f)-1) <= 0.001f && fabsf(sqrtf(u)-1) <= 0.001f && fabsf(dot) <= 0.001f &&
		isfinite(s->fov) && s->fov >= 0.2f && s->fov <= 2.6f && s->bsp >= 0 &&
		s->settle_ticks >= 0 && s->settle_ticks <= 600 && s->safe[0] < s->safe[2] && s->safe[1] < s->safe[3] &&
		(s->erase_ai == 0 || s->erase_ai == 1) && memchr(s->variant, 0, sizeof(s->variant));
}

/* A bounded JSON reader. Token subtrees end at next, so object lookup cannot
   accidentally pick a field from another map or shot. Escaped names/variants
   are refused rather than interpreted as a different engine identifier. */
struct token { const char *start; int length, next; char type; };
struct json { const char *p; struct token t[2048]; int count, depth, bad; };
static void space(struct json *j) { while (isspace((unsigned char)*j->p)) j->p++; }
static int value(struct json *j)
{
	int index = ++j->count;
	struct token *t;
	char close;
	if (index >= 2048 || ++j->depth > 24) { j->bad = 1; return 0; }
	space(j); t = &j->t[index]; t->start = j->p; t->type = *j->p;
	if (*j->p == '{' || *j->p == '[')
	{
		close = *j->p++ == '{' ? '}' : ']'; space(j);
		if (*j->p != close) for (;;)
		{
			if (close == '}')
			{
				space(j); if (*j->p != '"') { j->bad = 1; break; }
				value(j); space(j); if (*j->p++ != ':') { j->bad = 1; break; }
			}
			value(j); space(j);
			if (j->bad || *j->p != ',') break;
			j->p++;
		}
		if (!j->bad && *j->p == close) j->p++; else j->bad = 1;
	}
	else if (*j->p == '"')
	{
		j->p++;
		while (*j->p && *j->p != '"')
		{
			if ((unsigned char)*j->p < 32 || *j->p == '\\') { j->bad = 1; break; }
			j->p++;
		}
		if (!j->bad && *j->p == '"') j->p++; else j->bad = 1;
	}
	else
	{
		const char *begin = j->p;
		char *end;
		if (*j->p == '-') j->p++;
		if (*j->p == '0') j->p++;
		else if (*j->p >= '1' && *j->p <= '9') while (isdigit((unsigned char)*j->p)) j->p++;
		else j->bad = 1;
		if (*j->p == '.') { j->p++; if (!isdigit((unsigned char)*j->p)) j->bad = 1; while (isdigit((unsigned char)*j->p)) j->p++; }
		if (*j->p == 'e' || *j->p == 'E') { j->p++; if (*j->p == '+' || *j->p == '-') j->p++; if (!isdigit((unsigned char)*j->p)) j->bad = 1; while (isdigit((unsigned char)*j->p)) j->p++; }
		if (!isfinite(strtod(begin, &end)) || end != j->p) j->bad = 1;
		t->type = 'n';
	}
	t->length = (int)(j->p - t->start); t->next = j->count + 1; j->depth--;
	return index;
}
static int equal(struct json *j, int i, const char *s)
{
	return i && j->t[i].type == '"' && j->t[i].length == (int)strlen(s)+2 && !strncmp(j->t[i].start+1, s, strlen(s));
}
static int field(struct json *j, int object, const char *name)
{
	int i, found = 0;
	if (!object || j->t[object].type != '{') return 0;
	for (i = object+1; i < j->t[object].next; i = j->t[i+1].next)
	{
		if (equal(j, i, name)) { if (found) return 0; found = i+1; }
	}
	return found;
}
static int integer(struct json *j, int i, int *out)
{
	double v;
	if (!i || j->t[i].type != 'n') return 0;
	/* Require JSON integer syntax, including for schema and revision. */
	{ int k; for (k = 0; k < j->t[i].length; k++) if (strchr(".eE", j->t[i].start[k])) return 0; }
	v = strtod(j->t[i].start, NULL);
	if (v < 0 || v > INT_MAX) return 0;
	*out = (int)v; return 1;
}
static int array(struct json *j, int i, float *out, int count)
{
	int k, item = i+1;
	if (!i || j->t[i].type != '[') return 0;
	for (k = 0; k < count; k++, item++)
	{
		if (item >= j->t[i].next || j->t[item].type != 'n') return 0;
		out[k] = (float)strtod(j->t[item].start, NULL);
	}
	return item == j->t[i].next;
}
static int shot_read(struct json *j, int i, struct photo_shot *s)
{
	int fov = field(j, i, "fov"), scene = field(j, i, "scene"), variant = field(j, scene, "variant"), ai = field(j, scene, "ai");
	if (!array(j, field(j,i,"position"), s->position, 3) || !array(j,field(j,i,"forward"),s->forward,3) ||
		!array(j,field(j,i,"up"),s->up,3) || !array(j,field(j,i,"focus"),s->focus,2) || !array(j,field(j,i,"safe"),s->safe,4) ||
		!integer(j,field(j,i,"bsp"),&s->bsp) || !integer(j,field(j,i,"settle_ticks"),&s->settle_ticks) ||
		!fov || j->t[fov].type != 'n' || !variant || j->t[variant].type != '"' || j->t[variant].length > 65 ||
		(!equal(j,ai,"erase") && !equal(j,ai,"keep"))) return 0;
	s->fov = (float)strtod(j->t[fov].start,NULL); s->erase_ai = equal(j,ai,"erase");
	memcpy(s->variant,j->t[variant].start+1,j->t[variant].length-2); s->variant[j->t[variant].length-2] = 0;
	return photo_shot_valid(s);
}
int photo_fragment_read(const char *path, const char *key, struct photo_shot *shot, int *revision)
{
	char *text = malloc(65537);
	struct json *j = calloc(1,sizeof(*j));
	FILE *file = fopen(path,"r");
	int ok = 0, schema, map, shots, i, count = 0;
	size_t length;
	struct photo_shot next;
	if (!file || !text || !j || !key_valid(key)) goto done;
	length = fread(text,1,65536,file); text[length] = 0;
	if (ferror(file) || !feof(file) || strlen(text) != length) goto done;
	j->p = text; value(j); space(j);
	if (j->bad || *j->p || !integer(j,field(j,1,"schema"),&schema) || schema != 1) goto done;
	map = field(j,field(j,1,"maps"),key);
	if (!integer(j,field(j,map,"revision"),revision) || *revision < 1) goto done;
	shots = field(j,map,"shots");
	if (!shots || j->t[shots].type != '[') goto done;
	for (i=shots+1; i<j->t[shots].next; i=j->t[i].next)
	{
		if (++count > 8 || !shot_read(j,i,&next)) goto done;
		if (count == 1) *shot = next;
	}
	ok = count > 0;
 done:
	if (file) fclose(file);
	free(text); free(j); return ok;
}
static void vector_write(FILE *f, const float *v, int count)
{
	int i; fputc('[',f);
	for(i=0;i<count;i++) fprintf(f,"%s%.9g",i ? ", " : "",v[i]);
	fputc(']',f);
}
int photo_fragment_write(const char *path, const char *key, const struct photo_shot *s)
{
	struct photo_shot old;
	int revision = 0, ok;
	char temp[1024];
	FILE *f;
	if (!key_valid(key) || !photo_shot_valid(s)) return 0;
	/* Do not silently reset a damaged file's revision. */
	f = fopen(path,"r");
	if (f) { fclose(f); if (!photo_fragment_read(path,key,&old,&revision)) return 0; }
	if (revision == INT_MAX || snprintf(temp,sizeof(temp),"%s.tmp",path) >= (int)sizeof(temp)) return 0;
	f = fopen(temp,"w"); if (!f) return 0;
	fprintf(f,"{\n  \"schema\": 1,\n  \"maps\": {\n    \"%s\": {\n      \"revision\": %d,\n      \"shots\": [{\n",key,revision+1);
	fputs("        \"position\": ",f); vector_write(f,s->position,3);
	fputs(",\n        \"forward\": ",f); vector_write(f,s->forward,3);
	fputs(",\n        \"up\": ",f); vector_write(f,s->up,3);
	fprintf(f,",\n        \"fov\": %.9g,\n        \"bsp\": %d,\n        \"focus\": ",s->fov,s->bsp); vector_write(f,s->focus,2);
	fputs(",\n        \"safe\": ",f); vector_write(f,s->safe,4);
	fprintf(f,",\n        \"settle_ticks\": %d,\n        \"scene\": {\"ai\": \"%s\", \"variant\": \"",s->settle_ticks,s->erase_ai ? "erase" : "keep");
	/* Engine variant names are printable identifiers, never arbitrary JSON text. */
	{ const unsigned char *p = (const unsigned char *)s->variant;
	  for (; *p; p++) { if (*p < 32 || *p == '"' || *p == '\\') { fclose(f); remove(temp); return 0; } fputc(*p,f); } }
	fputs("\"}\n      }]\n    }\n  }\n}\n",f);
	ok = !ferror(f); if (fclose(f)) ok = 0;
	if (ok && !rename(temp,path)) return 1;
	remove(temp); return 0;
}
int photo_camera_read(const char *path, struct photo_shot *s)
{
	FILE *f = fopen(path,"r"); int n, ch;
	if (!f) return 0;
	n = fscanf(f,"%f %f %f %f %f %f %f %f %f %f", &s->position[0],&s->position[1],&s->position[2],
		&s->forward[0],&s->forward[1],&s->forward[2],&s->up[0],&s->up[1],&s->up[2],&s->fov);
	do { ch = fgetc(f); } while (ch != EOF && isspace(ch));
	fclose(f); return n == 10 && ch == EOF && photo_shot_valid(s);
}

static float photo_fly_axis(unsigned long keys, unsigned long positive, unsigned long negative)
{
	return ((keys & positive) ? 1.f : 0.f) - ((keys & negative) ? 1.f : 0.f);
}

void photo_fly_step(unsigned long keys, float seconds, int speed_step, float delta[3])
{
	float rate = seconds > 0.f ? PHOTO_FLY_SPEED * powf(2.f, (float)speed_step) * seconds : 0.f;
	delta[0] = photo_fly_axis(keys, PHOTO_FLY_FORWARD, PHOTO_FLY_BACKWARD) * rate;
	delta[1] = photo_fly_axis(keys, PHOTO_FLY_LEFT, PHOTO_FLY_RIGHT) * rate;
	delta[2] = photo_fly_axis(keys, PHOTO_FLY_UP, PHOTO_FLY_DOWN) * rate;
}
