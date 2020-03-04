package com.example.myapplication;

import com.example.myapplication.utils.CLogUtils;
import com.lody.whale.xposed.XC_MethodHook;

/**
 * Created by Lyh on
 * 2020/1/17
 */
public class Hook extends XC_MethodHook {
    @Override
    protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
        super.beforeHookedMethod(param);
        CLogUtils.e("beforeHookedMethod");
    }

    @Override
    protected void afterHookedMethod(MethodHookParam param) throws Throwable {
        super.afterHookedMethod(param);
        CLogUtils.e("afterHookedMethod");
        param.setResult("被Hook");
    }
}
