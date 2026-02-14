export * from './node_modules/registry-js/dist/lib/registry';

export declare function deleteFromPath(hive:  string, subPath: string): boolean;
export declare function readFromKey(
  hive:  string,
  subPath: string
): {
  name: string;
  type: string;
  data: string | number;
};
