#include "../mem.h"
#include "event_pcm.h"

struct event_pcm {
  struct event_pipe *pipe;
  struct pcm_spec_builder *builder;
};

struct event_pipe*
event_pcm_get_pipe(struct event_pcm *pcm) {
  assert(pcm != NULL);
  return pcm->pipe;
}

error_t
event_pcm_get_spec(struct event_pcm *pcm, struct pcm_spec **spec_r) {
  assert(pcm != NULL);
  return pcm_spec_get(pcm->builder, spec_r);
}

error_t
event_pcm_create(
  struct event_pipe *source,
  struct event_pipe_config *config,
  struct event_pcm **pcm_r,
  struct pcm_spec_builder **spec_r) {
    assert(pcm_r != NULL);
    assert(spec_r != NULL);

    struct event_pipe *pipe = NULL;
    struct pcm_spec_builder *spec = NULL;
    struct event_pcm *pcm = NULL;
    error_t error_r = event_pipe_create(source, config, &pipe);
    if (error_r == 0) {
      error_r = pcm_spec_builder_create(&spec);
    }
    if (error_r == 0) {
      error_r = mem_calloc(
        "event_pcm",
        sizeof(struct event_pcm),
        (void**)&pcm); // NOLINT
    }

    if (error_r == 0) {
      pcm->builder = spec;
      pcm->pipe = pipe;
      *pcm_r = pcm;
      *spec_r = spec;
    } else {
      event_pipe_release(&pipe);
      pcm_spec_builder_release(&spec);
      mem_free((void**)&pcm); // NOLINT
    }
    return error_r;
  }
