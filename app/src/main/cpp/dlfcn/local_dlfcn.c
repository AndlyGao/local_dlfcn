//
// Created by luoyesiqiu
//

#include "local_dlfcn.h"
#ifdef __LP64__
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym  Elf64_Sym
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym  Elf32_Sym
#endif

/**
 * 打开一个本地so文件
 * @param lib_path so文件路径
 * @return 句柄
 */
void *local_dlopen(const char* lib_path){
    int fd = open(lib_path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    size_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        return NULL;
    }

    Elf_Ehdr *elf = (Elf_Ehdr *) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    fd = -1;

    if (elf == MAP_FAILED) {
        return NULL;
    }

    char* shoff = ((char *) elf) + elf->e_shoff;
    struct local_dlfcn_handle* handle = (struct local_dlfcn_handle *) calloc(1,sizeof(struct local_dlfcn_handle)) ;

    for (int k = 0; k < elf->e_shnum; k++, shoff += elf->e_shentsize) {

        Elf_Shdr *sh = (Elf_Shdr *) shoff;
        LOGD("%s: k=%d shdr=%p type=%x", __func__, k, sh, sh->sh_type);

        switch (sh->sh_type) {
            case SHT_DYNSYM:
                handle->dynsym = malloc(sh->sh_size);
                if (!handle->dynsym) {
                    return NULL;
                }
                memcpy(handle->dynsym, ((char *) elf) + sh->sh_offset, sh->sh_size);
                handle->nsyms = (sh->sh_size / sizeof(Elf_Sym));
                LOGD("%s SHT_DYNSYM",__func__);
                break;

            case SHT_STRTAB:
                if (handle->dynstr) break;    /* .dynstr is guaranteed to be the first STRTAB */
                handle->dynstr = malloc(sh->sh_size);
                if (!handle->dynstr) {
                    goto tail;
                }
                memcpy(handle->dynstr, ((char *) elf) + sh->sh_offset, sh->sh_size);
                LOGD("%s SHT_STRTAB",__func__);
                break;
        }
    }
    munmap(elf, size);
    elf = 0;

    return handle;
    tail:
    if (fd >= 0) close(fd);
    if (elf != MAP_FAILED) munmap(elf, size);
    local_dlclose(handle);
    return NULL;
}

/**
 * 从so文件查找一个符号
 * @param handle 句柄
 * @param sym_name 符号名
 * @return 符号在so文件中的偏移
 */
off_t local_dlsym(void *handle,const char *sym_name){
    struct local_dlfcn_handle* h = (struct local_dlfcn_handle*)handle;
    Elf_Sym *sym = (Elf_Sym *) h->dynsym;
    char *strings = h->dynstr;
    for(int i = 0; i < h->nsyms;sym++,i++){
        if (strcmp(strings + sym->st_name, sym_name) == 0) {
            off_t ret = sym->st_value;
            LOGD("%s %s found at %p,bias = %ld",__func__, sym_name, ret);
            return ret;
        }
    }
    return -1;
}

/**
 * 关闭句柄
 * @param handle 句柄
 */
void local_dlclose(void *handle){
    if(handle){
        struct local_dlfcn_handle *h = (struct local_dlfcn_handle *) handle;
        if(h->dynstr) free(h->dynstr);
        if(h->dynsym) free(h->dynsym);
        free(handle);
    }
}