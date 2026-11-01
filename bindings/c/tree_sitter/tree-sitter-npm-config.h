#ifndef TREE_SITTER_NPM_CONFIG_H_
#define TREE_SITTER_NPM_CONFIG_H_

typedef struct TSLanguage TSLanguage;

#ifdef __cplusplus
extern "C" {
#endif

const TSLanguage *tree_sitter_npmrc(void);

#ifdef __cplusplus
}
#endif

#endif // TREE_SITTER_NPM_CONFIG_H_
