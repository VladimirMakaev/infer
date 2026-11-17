type 'a singleton_or_more = Empty | Singleton of 'a | More

let fold_of_pervasives_map_fold fold m ~init ~f = fold (fun k v acc -> f acc (k, v)) m init

let fold_of_pervasives_set_fold fold s ~init ~f = fold (fun x acc -> f acc x) s init
