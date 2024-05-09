#include "alg3.h"
#include "utility.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int start;
    int end;
    GArray *minimizers;
    GHashTable *k_finger_occurrences;
    int k;
    FILE *fp;
    GArray *lengths;
    GArray *read_ids;
} ThreadArgs;

GArray* get_k_fingers(char *line, char **read_id){
    GArray *array = g_array_new(FALSE, FALSE, sizeof(int));

    char *p = strtok(line, " "); //skip name
    char *read = malloc(sizeof(char) * strlen(p) + 1);
    strcpy(read, p);
    *read_id = read;
    p = strtok(NULL, " ");

    while(p != NULL){
        if(strcmp(p,"|") != 0 && is_numeric(p)){
            int num = atoi(p);
            g_array_append_val(array, num);
        }

        p = strtok(NULL, " ");
    }

    return array;
}

char *k_finger_to_string(GArray *array, int i, int k, const char *separator) {

    if (array == NULL || separator == NULL) {
        return NULL;
    }

    size_t total_length = 0;
    int separator_length = strlen(separator);
    for (int x = i; x < i+k; x++) {
        int number_length = snprintf(NULL, 0, "%d", g_array_index(array, int, x));
        if (number_length < 0) {
            return NULL;
        }
        total_length += number_length+separator_length;
    }
    total_length -= separator_length;

    char *result = (char *)malloc(total_length + 1);
    if (result == NULL) {
        return NULL;
    }

    int offset = 0;
    for (int x = i; x < i+k; x++) {
        int chars_written = snprintf(result + offset, total_length - offset + 1, "%d",
                                     g_array_index(array, int, x));
        if (chars_written < 0 || chars_written > total_length - offset) {
            free(result);
            return NULL;
        }
        offset += chars_written;
        if (x < i + k - 1) {
            // Append separator if not the last element
            strcpy(result + offset, separator);
            offset += separator_length;
        }
    }

    return result;
}

GArray* alg3(GArray* fingerprint, int w, int k, int (*phi)(GArray *array, int i, int k),
             void insertt(GArray *array, GQueue *queue, Element *X, int (*phi)(GArray *array, int i, int k), int k),
             int n) {
    GArray *rfinger;

    rfinger = g_array_new(0, 0, sizeof(Element *));

    GQueue *queue;

    queue = g_queue_new();

    int length = 0;

    for (int x = 0; x < n - k; x++) {
        Element *el = malloc(sizeof(Element));

        int k_finger = g_array_index(fingerprint, int, x);

        length += k_finger;

        el->value = x;
        el->fingerprint = NULL;
        el->k_finger = supporting_length(fingerprint, x, k);
        el->index_offset = length - k_finger;

        insertt(fingerprint, queue, el, supporting_length, k);
        //print_queue(queue);

        if (x >= w - 1) {
            Element *fetched = fetch(queue, x - w + 1);

            if(!(rfinger->len) ||
               (g_array_index(rfinger, Element *, rfinger->len - 1))->value != fetched->value){

                if(supporting_length(fingerprint, fetched->value, k) >= MIN_SUP_LENGTH){
                    char *result = k_finger_to_string(fingerprint, fetched->value, k, "_");
                    fetched->fingerprint = result;

                    g_array_append_val(rfinger, fetched);
                }
            }
        }
    }

    for(int x = n-k; x<n; x++){
        length += g_array_index(fingerprint, int, x);
    }


    while (!g_queue_is_empty(queue)) {
        Element *data = (Element *)g_queue_pop_head(queue);
        if(data->fingerprint == NULL)
            free(data);
    }

    g_queue_free(queue);

    g_array_append_val(fingerprint, length);

    return rfinger;

}

GHashTable *compute_k_finger_occurrences(GArray *fingerprint_list){
    GHashTable *hash_table = g_hash_table_new(g_str_hash, g_str_equal);

    for(int x=0; x<fingerprint_list->len ; x++){
        GArray* arr = g_array_index(fingerprint_list, GArray *, x);

        for(int y=0; y<arr->len ; y++){
            Element *val = g_array_index(arr, Element *, y);
            GArray *retrived = g_hash_table_lookup(hash_table, val->fingerprint);

            if(retrived == NULL)
                retrived = g_array_new(FALSE, FALSE, sizeof(Duo_int *));

            Duo_int *new = malloc(sizeof(Duo_int));
            new->first = x;
            new->second = val->value;
            new->third = val->index_offset;
            new->fourth = val->k_finger;

            g_array_append_val(retrived, new);

            g_hash_table_insert(hash_table, val->fingerprint, retrived);

            //free(val);
        }
        //g_array_free(arr, TRUE);
    }

    return hash_table;
}

gboolean filter_hash_table(gpointer key, gpointer value, gpointer user_data) {
    GArray *val = (GArray *)value;
    if(val->len > 1 && (MAX_K_FINGER_OCCURRENCE == -1 || val->len <= MAX_K_FINGER_OCCURRENCE))
        return FALSE;

    //free_key_occurrences(key, value, NULL);
    free_garray_duo_int(value);

    return TRUE;
}

int compare_Triple_int(const void *f, const void *s) {
    Triple_int *a = *((Triple_int **)f);
    Triple_int *b = *((Triple_int **)s);

    if (a->first < b->first)
        return -1;
    if (a->first > b->first)
        return 1;

    if (a->second->value < b->second->value)
        return -1;
    if (a->second->value > b->second->value)
        return 1;

    return 0;
}

void *thread_matches(void *args) {
    ThreadArgs *thread_args = (ThreadArgs *)args;

    compute_matches(thread_args->minimizers,thread_args->k_finger_occurrences
                    ,thread_args->k, thread_args->fp
                    ,thread_args->lengths, thread_args->read_ids
                    ,thread_args->start, thread_args->end);

    pthread_exit(NULL);
}

void compute_matches(GArray *minimizers, GHashTable *k_finger_occurrences, int k, FILE *fp
                      ,GArray *lenghts, GArray *read_ids, int start_thread, int end_thread){
    for(int x=start_thread; x<=end_thread; x++){
        GArray *current = g_array_index(minimizers, GArray *, x);
        GArray *Arr = g_array_new(FALSE, FALSE, sizeof(Triple_int *));
        for(int y=0; y < current->len; y++){
            Element *current_element = g_array_index(current, Element *, y);
            GArray *occ_list = (GArray *)g_hash_table_lookup(k_finger_occurrences, current_element->fingerprint);

            if(occ_list == NULL)
                continue;

            for(int z=0; z<occ_list->len;z++){
                Duo_int *value = g_array_index(occ_list, Duo_int *, z);

                if(value->first <= x)
                    continue;

                Triple_int *new = malloc(sizeof(Triple_int));
                new->first = value->first;
                new->second = current_element;
                new->third = value;

                g_array_append_val(Arr, new);
            }
        }

        g_array_sort(Arr, compare_Triple_int);

        //print_array_Triple_int(Arr);

        int start = 0;
        Triple_int *value;
        Triple_int *old_value = NULL;
        for(int end=0; end<Arr->len; end++){
            value = g_array_index(Arr, Triple_int *, end);
            //printf("(%d,%d,%d)\n", value->first, value->second->value, value->third->second);

            if((old_value != NULL && value->first != old_value->first) || end == Arr->len - 1){

                if(end - start >= MIN_SHARED_K_FINGERS){
                    offset_struct o;

                    int score = maximal_colinear_subset(Arr, start, end, k, &o);

                    o.number = score/k;

                    //print_offset_struct(o);

                    find_overlap(x, old_value->first, &o, fp, lenghts, read_ids);

                    //printf("\n score: %d", score);

                }
                free_partial_GArray(Arr, start, end);
                start = end;
                old_value = NULL;
            }
            else{
                old_value = value;
            }
        }

        if(Arr->len)
            free(g_array_index(Arr, Triple_int *, Arr->len-1));

        g_array_free(Arr, TRUE);
        free_garray_of_pointers(current);
    }
}

void find_overlap(int first, int second, offset_struct *current, FILE *fp
                  , GArray *lenghts, GArray *read_ids) {
    if(current->number >= MIN_CHAIN_LENGTH){
        if(current->right_offset2 >= current->left_offset2){

            //length offset for left fingerprint
            int upstream_length1 = current->left_index_offset1;
            //length offset for right fingerprint
            int upstream_length2 = current->left_index_offset2;

            //supporting length left fingerprint
            int region_length1 = current->right_index_offset1 + current->right_supp_length1 - current->left_index_offset1;
            //supporting length right fingerprint
            int region_length2 = current->right_index_offset2 + current->right_supp_length2 - current->left_index_offset2;

            int read1_length = g_array_index(lenghts, int, first);
            int read2_length = g_array_index(lenghts, int, second);

            int min_cov_number = (int)((MIN_REGION_K_FINGER_COVERAGE * min(region_length1,region_length2)) / MIN_SUP_LENGTH);
            min_cov_number = min(min_cov_number, 15);

            if (current->number >= min_cov_number  &&
                (abs(region_length1-region_length2) <= MAX_DIFF_REGION_PERCENTAGE * max(region_length1,region_length2)
                 && max(region_length1,region_length2) >= MIN_REGION_LENGTH)){

                int min_up = min(upstream_length1,upstream_length2);

                int start_ov1 = upstream_length1 - min_up;
                int start_ov2 = upstream_length2 - min_up;

                int min_down = min(read1_length-(upstream_length1+region_length1), read2_length-(upstream_length2+region_length2));

                int end_ov1 = upstream_length1 + region_length1 + min_down;
                int end_ov2 = upstream_length2 + region_length2 + min_down;

                int ov_length = min(end_ov1-start_ov1, end_ov2-start_ov2);

                if (min(region_length1,region_length2) >= MIN_OVERLAP_COVERAGE * ov_length && ov_length >= MIN_OVERLAP_LENGTH){
                    Duo_char *dd = malloc(sizeof(Duo_char));

                    char *r1  = g_array_index(read_ids, char *, first);
                    int f_len = strlen(r1);
                    dd->first = substring(r1, 0, f_len-3);     //remove last two chars blahblah_0

                    char *r2  = g_array_index(read_ids, char *, second);
                    int s_len = strlen(r2);
                    dd->second = substring(r2, 0, s_len-3);     //remove last two chars blahblah_0

                    int val[9];
                    val[0] = r1[f_len-1] - '0';
                    val[1] = r2[s_len-1] - '0';
                    val[2] = read1_length;
                    val[3] = read2_length;
                    val[4] = start_ov1;
                    val[5] = end_ov1;
                    val[6] = start_ov2;
                    val[7] = end_ov2;
                    val[8] = ov_length;

                    print_PAF_minimap(dd, val, fp);

                    free(dd->first);
                    free(dd->second);
                    free(dd);
                }
            }
        }
    }

}

int main(void){
    struct rusage usage;
    double convert = 1e-6;
    FILE *fp;
    FILE *output = OUTPUT;
    ssize_t read = 0;
    char *line = NULL;
    size_t len = 0;

    clock_t begin_total = clock();

    fp = INPUT;

    if (fp == NULL){
        printf("Error Reading File\n");
        exit(1);
    }

    GArray *minimizers = g_array_new(FALSE, FALSE, sizeof(GArray*));

    GArray *lengths = g_array_new(FALSE, FALSE, sizeof(int));

    GArray *read_ids = g_array_new(FALSE, FALSE, sizeof(char *));

    clock_t begin = clock();

    while ((read = getline(&line, &len, fp)) != -1) {
        char *read_id;

        GArray *array = get_k_fingers(line, &read_id);

        GArray* res = alg3(array, W, K, supporting_length, insertLex, array->len);

        g_array_append_val(read_ids, read_id);

        int le = g_array_index(array, int, array->len-1);

        g_array_append_val(lengths, le);

        g_array_append_val(minimizers, res);

        g_array_free(array, TRUE);
    }

    fclose(fp);

    clock_t end = clock();

    calculate_usage(&usage);

    printf("Number of Reads %d, Minimizers calculated in %f s, Memory: %.2g GB \n", minimizers->len, (double)(end - begin) / CLOCKS_PER_SEC, usage.ru_maxrss*convert);

    /* GArray* first = g_array_index(minimizers, GArray *, 0); */
    /* GArray* second = g_array_index(minimizers, GArray *, 1); */
    /* GArray* third = g_array_index(minimizers, GArray *, 2); */
    /* GArray* fourth = g_array_index(minimizers, GArray *, 3); */
    /* GArray* fifth = g_array_index(minimizers, GArray *, 4); */
    /* GArray* sixth = g_array_index(minimizers, GArray *, 5); */
    /* GArray* s7 = g_array_index(minimizers, GArray *, 6); */
    /* GArray* s8 = g_array_index(minimizers, GArray *, 7); */
    /* GArray* s9 = g_array_index(minimizers, GArray *, 8); */
    /* GArray* s10 = g_array_index(minimizers, GArray *, 9); */

    /* printf("%d %d %d %d %d %d %d %d %d %d\n", first->len, second->len, third->len, fourth->len, fifth->len, */
    /*        sixth->len,s7->len,s8->len,s9->len,s10->len); */

    /* print_Element(g_array_index(first, Element *, 0)); */
    /* puts(""); */

    // DICTIONARY

    GHashTable *k_finger_occurrences = compute_k_finger_occurrences(minimizers);

    //g_array_free(minimizers, TRUE);

    //GArray *arr = g_hash_table_lookup(k_finger_occurrences, "3_1_3_14_19_3_1");

    //Eliminare le k-fingers che occorrono una volta sola nel set dei reads oppure che occorrono troppe volte.
    //max_k_finger_occurrence se valore -1 non controllare massimo

    calculate_usage(&usage);

    guint num_before = g_hash_table_size(k_finger_occurrences);
    printf("Number of keys in k_finger_occurrences before filtering table: %u, Memory: %.2g GB \n", num_before, usage.ru_maxrss*convert);

    // filter
    g_hash_table_foreach_remove(k_finger_occurrences, filter_hash_table, NULL);

    calculate_usage(&usage);

    guint num_after = g_hash_table_size(k_finger_occurrences);
    printf("Number of keys in k_finger_occurrences after filtering table: %u, Memory: %.2g GB \n", num_after, usage.ru_maxrss*convert);

    // Dizionari delle leftmost e rightmost k-fingers comuni

    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    int move = 0;
    int partition_size = minimizers->len / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; ++i) {
        move += minimizers->len / (2 << i);   //geometric series
        args[i].end = (i == 0) ? (minimizers->len - 1) : (args[i-1].start - 1);
        args[i].start = (i == NUM_THREADS - 1) ? 0 : minimizers->len - move;

        //args[i].start = i * partition_size;
        //args[i].end = (i == NUM_THREADS - 1) ? (minimizers->len - 1) : (args[i].start + partition_size - 1);


        args[i].minimizers = minimizers;
        args[i].k_finger_occurrences = k_finger_occurrences;
        args[i].k = K;
        args[i].fp = output;
        args[i].lengths = lengths;
        args[i].read_ids = read_ids;

        printf("(%d, %d)\n", args[i].start, args[i].end);

        pthread_create(&threads[i], NULL, thread_matches, (void *)&args[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    g_array_free(minimizers, TRUE);

    g_hash_table_foreach(k_finger_occurrences, free_key_occurrences, NULL);
    g_hash_table_destroy(k_finger_occurrences);
    g_array_free(lengths, TRUE);
    free_garray_string(read_ids);

    clock_t end_total = clock();

    calculate_usage(&usage);

    printf("\nOverall time %f s, Memory: %.2g GB \n", (double)(end_total - begin_total) / CLOCKS_PER_SEC, usage.ru_maxrss*convert);

    return 0;
}
